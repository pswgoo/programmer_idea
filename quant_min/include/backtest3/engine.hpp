#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

#include "backtest3/worker_pool.hpp"
#include "backtest3/symbol_context.hpp"
#include "backtest3/portfolio.hpp"
#include "backtest3/strategy_portfolio.hpp"

namespace bt3 {

inline std::size_t owner_worker(std::size_t sym_idx, std::size_t n_workers) {
  return sym_idx % n_workers; // 之后你可以做 NUMA grouping
}

struct EngineConfig {
  std::size_t n_workers{4};
};

class MultiSymbolEngine {
public:
  MultiSymbolEngine(std::size_t n_symbols,
                    EngineConfig cfg,
                    const bt::ExecConfig& exec_cfg,
                    const bt::RiskConfig& risk_cfg)
      : n_(n_symbols),
        pool_(cfg.n_workers),
        ctx_(n_),
        portfolio_(n_),
        mvs_(n_),
        per_sym_bucket_(n_),
        per_sym_cancel_cmds_(n_),
        per_sym_submit_cmds_(n_),
        worker_syms_(pool_.size()) {

    for (auto& c : ctx_) {
      c.set_exec_config(exec_cfg);
      c.set_risk_config(risk_cfg);
    }

    for (std::size_t i = 0; i < n_; ++i) {
      worker_syms_[owner_worker(i, pool_.size())].push_back(i);
    }
  }

  template <class SchedulerT, class StrategyT>
  void run(SchedulerT& sched, StrategyT& strat) {
    std::int64_t last_ts = -1;
    std::size_t batches = 0;
    std::size_t events = 0;
    std::size_t fills_cnt = 0;

    // jobs vectors reused
    std::vector<std::function<void(std::size_t)>> jobsA(pool_.size());
    std::vector<std::function<void(std::size_t)>> jobsB(pool_.size());

    while (sched.has_next()) {
      auto batch = sched.next_batch_same_ts(); // vector<pair<sym_idx, MarketEvent>>
      if (batch.empty()) break;

      const auto ts = batch.front().second.ts_ns;
      last_ts = ts;
      ++batches;
      events += batch.size();

      // 0) build per-symbol bucket (main thread)
      for (auto& b : per_sym_bucket_) b.clear();
      for (auto const& it : batch) {
        per_sym_bucket_[it.first].push_back(it.second);
      }

      // ====================== Phase A: Market ======================
      for (std::size_t wid = 0; wid < pool_.size(); ++wid) {
        jobsA[wid] = [this, ts](std::size_t wid_) {
          for (auto sym_idx : worker_syms_[wid_]) {
            auto& bucket = per_sym_bucket_[sym_idx];
            if (bucket.empty()) continue;
            ctx_[sym_idx].process_market_events(bucket, ts);
            // ctx_[sym_idx].last_mv is ready
          }
        };
      }
      pool_.run_all(jobsA);

      // barrier A -> main thread: apply fills + updates, build mvs snapshot
      for (std::size_t i = 0; i < n_; ++i) {
        mvs_[i] = ctx_[i].last_mv;

        for (auto const& fe : ctx_[i].fills) {
          portfolio_.apply_fill(i, fe);
          ++fills_cnt;
        }
        for (auto const& up : ctx_[i].updates) {
          strat.on_order_updated(i, ctx_[i].oms, up);
        }
      }

      // ====================== Strategy Decision ======================
      auto decision = strat.on_batch(mvs_, portfolio_);

      // 1) build per-symbol command lists (main thread)
      for (auto& v : per_sym_cancel_cmds_) v.clear();
      for (auto& v : per_sym_submit_cmds_) v.clear();

      for (auto const& c : decision.cancels) {
        if (c.sym_idx >= n_) continue;
        per_sym_cancel_cmds_[c.sym_idx].push_back(typename SymbolContext::CancelCmd{c.order_id});
      }
      for (auto const& s : decision.submits) {
        if (s.sym_idx >= n_) continue;
        per_sym_submit_cmds_[s.sym_idx].push_back(typename SymbolContext::SubmitCmd{s.req});
      }

      // ====================== Phase B: Orders ======================
      for (std::size_t wid = 0; wid < pool_.size(); ++wid) {
        jobsB[wid] = [this](std::size_t wid_) {
          for (auto sym_idx : worker_syms_[wid_]) {
            auto& cancels = per_sym_cancel_cmds_[sym_idx];
            auto& submits = per_sym_submit_cmds_[sym_idx];
            if (cancels.empty() && submits.empty()) continue;

            // 这里你也可以加入 symbol risk（因为它只读 portfolio 的 pos，需要主线程提供 snapshot pos）
            ctx_[sym_idx].process_commands(cancels, submits);
          }
        };
      }
      pool_.run_all(jobsB);

      // barrier B -> main thread: apply fills + updates (order acks/cancel req + immediate fills)
      for (std::size_t i = 0; i < n_; ++i) {
        for (auto const& fe : ctx_[i].fills) {
          portfolio_.apply_fill(i, fe);
          ++fills_cnt;
        }
        for (auto const& up : ctx_[i].updates) {
          // submit ack 的 tracking（如果你策略有这个接口）
          // 你也可以在 strategy.on_order_updated 内部通过 oms.get(order_id)->side 来推断
          strat.on_order_updated(i, ctx_[i].oms, up);
        }
      }

      if ((batches % 1000) == 0) {
        std::cout << "ts=" << last_ts
                  << " batches=" << batches
                  << " events=" << events
                  << " fills=" << fills_cnt
                  << " equity=" << portfolio_.equity(mvs_)
                  << "\n";
      }
    }

    std::cout << "DONE ts=" << last_ts
              << " batches=" << batches
              << " events=" << events
              << " fills=" << fills_cnt
              << " equity=" << portfolio_.equity(mvs_)
              << "\n";
  }

  Portfolio& portfolio() { return portfolio_; }

private:
  std::size_t n_;
  WorkerPool pool_;

  std::vector<SymbolContext> ctx_;

  Portfolio portfolio_;
  std::vector<MarketView> mvs_;

  // per-symbol market events at current ts
  std::vector<std::vector<q::market::MarketEvent>> per_sym_bucket_;

  // per-symbol commands at current ts
  std::vector<std::vector<typename SymbolContext::CancelCmd>> per_sym_cancel_cmds_;
  std::vector<std::vector<typename SymbolContext::SubmitCmd>> per_sym_submit_cmds_;

  // worker ownership table
  std::vector<std::vector<std::size_t>> worker_syms_;
};

} // namespace bt3
