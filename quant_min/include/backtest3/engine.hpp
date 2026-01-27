#pragma once
#include <cstdint>
#include <future>
#include <iostream>
#include <vector>

#include "backtest3/scheduler.hpp"
#include "backtest3/symbol_context.hpp"
#include "backtest3/thread_pool.hpp"
#include "backtest3/symbol_index.hpp"
#include "backtest3/portfolio.hpp"
#include "backtest3/multi_market_view.hpp"
#include "backtest3/portfolio_risk.hpp"
#include "backtest3/execution_sim.hpp"
#include "backtest3/strategy_pairs.hpp"
#include "backtest3/orders.hpp"
#include "backtest3/types.hpp"

namespace bt3 {

class MultiBacktestEngine {
public:
  MultiBacktestEngine(std::vector<Instrument> instruments,
                      std::vector<VectorReplay*> replays,
                      std::size_t n_threads,
                      PairsConfig strat_cfg,
                      PortfolioRiskConfig risk_cfg,
                      ExecSimConfig exec_cfg)
      : instruments_(std::move(instruments)),
        replays_(std::move(replays)),
        pool_(n_threads),
        sym_index_(instruments_),
        portfolio_(sym_index_.size()),
        strategy_(strat_cfg),
        risk_(risk_cfg),
        exec_(exec_cfg) {

    const auto n = sym_index_.size();
    sym_ctx_.resize(n);
    last_mv_.resize(n);

    for (std::size_t i = 0; i < n; ++i) {
      sym_ctx_[i].sym = sym_index_.id(i);
      last_mv_[i] = MarketView{};
    }
  }

  // helper: set initial positions by SymbolId
  void set_position(SymbolId sym, std::int64_t p) {
    portfolio_.set_pos(sym_index_.idx(sym), p);
  }

  void run() {
    KWayMergeScheduler sched(replays_);

    std::int64_t last_ts = -1;
    std::size_t total_events = 0;
    std::size_t total_batches = 0;
    std::size_t total_trades = 0;

    std::vector<int> seen(sym_index_.size(), -1);

    while (sched.has_next()) {
      auto raw = sched.next_batch_same_ts();
      if (raw.empty()) break;

      const auto ts = raw.front().ts_ns;
      last_ts = ts;
      ++total_batches;

      // ---- 0) per-symbol 聚合（同 ts 每 symbol 只保留最后一条） ----
      unique_work_.clear();
      for (const auto& ev : raw) {
        const auto idx = sym_index_.idx(ev.sym);
        if (seen[idx] != static_cast<int>(total_batches)) {
          seen[idx] = static_cast<int>(total_batches);
          unique_work_.push_back(WorkItem{idx, ev});
        } else {
          for (auto& w : unique_work_) {
            if (w.idx == idx) { w.ev = ev; break; }
          }
        }
      }

      // ---- 1) batch 并行更新市场状态（无锁） ----
      std::vector<std::future<void>> futs;
      futs.reserve(unique_work_.size());
      for (const auto& w : unique_work_) {
        futs.push_back(pool_.submit([this, w]() mutable {
          auto& ctx = sym_ctx_[w.idx];
          ctx.on_event(w.ev);
          last_mv_[w.idx] = ctx.last_mv;
        }));
      }
      for (auto& f : futs) f.get(); // barrier：这个 ts 的市场切片完成

      total_events += raw.size();

      // ---- 2) barrier 后：策略决策（组合级） ----
      if (risk_.killed()) {
        // 熔断后只更新净值，不再交易
        risk_.on_equity(portfolio_.equity(last_mv_));
        continue;
      }

      // 只演示交易一对：strat_cfg.idx_a / idx_b 对应的 symbol id
      const SymbolId sym_a = sym_index_.id(strategy_pair_a());
      const SymbolId sym_b = sym_index_.id(strategy_pair_b());

      MultiMarketView mmv;
      mmv.ts_ns = ts;
      mmv.mvs = &last_mv_;

      auto intents = strategy_.on_batch(mmv, portfolio_, sym_a, sym_b);

      // ---- 3) 组合风控 + 执行 + 更新组合 ----
      for (const auto& oi : intents) {
        const auto idx = sym_index_.idx(oi.sym);
        auto rd = risk_.pre_trade_check(portfolio_, last_mv_, idx, oi);
        if (!rd.ok) continue;

        auto fill_opt = exec_.try_execute(ts, last_mv_[idx], oi);
        if (!fill_opt) continue;

        auto f = *fill_opt;

        // fee（用 cash 体现）
        const auto fee = exec_.fee_for(f);
        portfolio_.apply_fill(idx, f);
        // fee 扣现金
        portfolio_.set_cash(idx, portfolio_.cash(idx) - fee);

        ++total_trades;
      }

      // ---- 4) 更新组合净值 & drawdown risk ----
      const double eq = portfolio_.equity(last_mv_);
      risk_.on_equity(eq);

      if ((total_batches % 1000) == 0) {
        std::cout << "ts=" << last_ts
                  << " batches=" << total_batches
                  << " events=" << total_events
                  << " trades=" << total_trades
                  << " equity=" << eq
                  << (risk_.killed() ? " [KILLED]" : "")
                  << "\n";
      }
    }

    std::cout << "DONE. last_ts=" << last_ts
              << " total_batches=" << total_batches
              << " total_events=" << total_events
              << " total_trades=" << total_trades
              << "\n";
  }

  Portfolio& portfolio() { return portfolio_; }

private:
  struct WorkItem {
    std::size_t idx{};
    TickEvent ev{};
  };

  std::size_t strategy_pair_a() const { return cfg_pair_a_; }
  std::size_t strategy_pair_b() const { return cfg_pair_b_; }

private:
  std::vector<Instrument> instruments_;
  std::vector<VectorReplay*> replays_;

  ThreadPool pool_;
  SymbolIndex sym_index_;

  std::vector<SymbolContext> sym_ctx_;
  std::vector<MarketView> last_mv_;

  Portfolio portfolio_;

  // Strategy/Risk/Exec
  PairsMeanReversionStrategy strategy_;
  PortfolioRisk risk_;
  ExecutionSim exec_;

  // batch workspace
  std::vector<WorkItem> unique_work_;

  // cache pair indices (from strategy config)
  std::size_t cfg_pair_a_{0};
  std::size_t cfg_pair_b_{1};
};

} // namespace bt3
