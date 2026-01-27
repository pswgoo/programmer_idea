#pragma once
#include <cstdint>
#include <future>
#include <iostream>
#include <vector>

#include "backtest3/scheduler.hpp"
#include "backtest3/symbol_context.hpp"
#include "backtest3/portfolio.hpp"
#include "backtest3/thread_pool.hpp"
#include "backtest3/symbol_index.hpp"
#include "backtest3/types.hpp"

namespace bt3 {

// Milestone 2 去锁版：
// - Scheduler 串行
// - 同 ts batch 内并行更新
// - per-symbol 状态与 last_mv 用 vector 存储（无锁）
// - batch 进入并行前先做 per-symbol 聚合，保证同 symbol 只处理一次
class MultiBacktestEngine {
public:
  MultiBacktestEngine(std::vector<Instrument> instruments,
                      std::vector<VectorReplay*> replays,
                      std::size_t n_threads)
      : instruments_(std::move(instruments)),
        replays_(std::move(replays)),
        pool_(n_threads),
        sym_index_(instruments_) {

    const auto n = sym_index_.size();
    sym_ctx_.resize(n);
    last_mv_.resize(n);
    portfolio_.resize(sym_index_.size());

    for (std::size_t i = 0; i < n; ++i) {
      sym_ctx_[i].sym = sym_index_.id(i);
      last_mv_[i] = MarketView{};
    }
  }

  void run() {
    KWayMergeScheduler sched(replays_);

    std::int64_t last_ts = -1;
    std::size_t total_events = 0;
    std::size_t total_batches = 0;

    // 用于 batch 内聚合：每个 symbol 在本 batch 是否出现
    std::vector<int> seen;
    seen.assign(sym_index_.size(), -1);

    while (sched.has_next()) {
      auto raw = sched.next_batch_same_ts();
      if (raw.empty()) break;

      const auto ts = raw.front().ts_ns;
      last_ts = ts;
      ++total_batches;

      // ---- 0) per-symbol 聚合（保证同 symbol 本 batch 只处理一次） ----
      // 策略：同一个 ts 同一个 symbol 多条事件，取“最后一条”（保持确定性）
      // 你之后换成真实 MarketEvent 时，这里可以改成“按 seq 逐条处理”，但那时就不能无锁写同槽位，需用 per-symbol 队列/串行。
      // 或者每个symbol用一个vector存所有的event，就可以保证每个symbol只在一个线程调用一次。
      unique_work_.clear();
      for (const auto& ev : raw) {
        const auto idx = sym_index_.idx(ev.sym);
        if (seen[idx] != static_cast<int>(total_batches)) {
          seen[idx] = static_cast<int>(total_batches);
          unique_work_.push_back(WorkItem{idx, ev});
        } else {
          // 已经有了，用最后一条覆盖（确定性：raw 顺序由 scheduler 决定）
          // 找到对应 WorkItem 并覆盖
          // unique_work_ 通常很小，用线性扫描即可；后面你可以用 idx->pos 表优化
          for (auto& w : unique_work_) {
            if (w.idx == idx) { w.ev = ev; break; }
          }
        }
      }

      // ---- 1) batch 内并行更新（无锁） ----
      std::vector<std::future<void>> futs;
      futs.reserve(unique_work_.size());

      for (const auto& w : unique_work_) {
        futs.push_back(pool_.submit([this, w]() mutable {
          auto& ctx = sym_ctx_[w.idx];
          ctx.on_event(w.ev);
          last_mv_[w.idx] = ctx.last_mv; // 无锁写：每 idx 只写一次
        }));
      }

      for (auto& f : futs) f.get(); // barrier

      total_events += raw.size();

      // ---- 2) barrier 后：策略/组合风控（Milestone 2 先留空位） ----

      if ((total_batches % 1000) == 0) {
        const double eq = equity_snapshot_unsafe(); // 读 last_mv_ 也无锁（barrier 后是稳定快照）
        std::cout << "ts=" << last_ts
                  << " batches=" << total_batches
                  << " total_events=" << total_events
                  << " unique_syms_in_batch=" << unique_work_.size()
                  << " equity=" << eq
                  << "\n";
      }
    }

    std::cout << "DONE. last_ts=" << last_ts
              << " total_events=" << total_events
              << " total_batches=" << total_batches
              << "\n";
  }

  Portfolio& portfolio() { return portfolio_; }

private:
  struct WorkItem {
    std::size_t idx{};
    TickEvent ev{};
  };

  double equity_snapshot_unsafe() const {
    return portfolio_.equity(last_mv_);
  }

private:
  std::vector<Instrument> instruments_;
  std::vector<VectorReplay*> replays_;

  ThreadPool pool_;
  SymbolIndex sym_index_;

  // 无锁连续存储
  std::vector<SymbolContext> sym_ctx_;
  std::vector<MarketView> last_mv_;

  // 组合状态
  Portfolio portfolio_;

  // batch 聚合工作区（主线程使用）
  std::vector<WorkItem> unique_work_;
};

} // namespace bt3
