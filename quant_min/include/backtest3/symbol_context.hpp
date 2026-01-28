#pragma once
#include <cstdint>
#include <vector>

// ===================== 项目二（真实 OMS / Exec / Risk）=====================
#include "backtest/orders.hpp"
#include "backtest/oms.hpp"
#include "backtest/execution.hpp"
#include "backtest/risk.hpp"
#include "backtest/market_view.hpp"
// ===========================================================================

// ===================== 项目一（MarketEvent / BookBuilder / Book）===========
#include "market/event.hpp"     // q::market::MarketEvent
#include "book/book_builder.hpp"       // BookBuilder<BookT>
#include "book/flat_l2_book.hpp"       // FlatL2Book (提供 top())
// ===========================================================================

namespace bt3 {

// 每个 SymbolContext 只由 owning worker 线程访问（OMS/Exec/Risk 线程绑定）
struct SymbolContext {
  // ---- market/book ----
  q::book::FlatL2Book book{};
  q::book::BookBuilder<q::book::FlatL2Book> builder{&book};

  // ---- project2 trading components ----
  bt::Oms oms{};
  bt::ExecutionSim exec{};
  bt::RiskManager risk{};

  // ---- cached view ----
  MarketView last_mv{};

  // ---- per-phase outputs (worker writes; main thread reads after barrier) ----
  std::vector<bt::FillEvent> fills;
  std::vector<bt::OrderUpdate> updates;

  // ---- configs ----
  void set_exec_config(const bt::ExecConfig& cfg) { exec = bt::ExecutionSim(cfg); }
  void set_risk_config(const bt::RiskConfig& cfg) { risk = bt::RiskManager(cfg); }

  // 从 book.top() 刷新 MarketView（你给的 top() 接口）
  void refresh_view(std::int64_t ts_ns) {
    last_mv.ts_ns = ts_ns;

    const auto t = book.top();
    if (!t.valid) {
      last_mv.best_bid_px = 0;
      last_mv.best_ask_px = 0;
      last_mv.mid_px = 0;
      return;
    }

    last_mv.best_bid_px = t.bid_px;
    last_mv.best_ask_px = t.ask_px;
    last_mv.mid_px = (t.bid_px > 0 && t.ask_px > 0) ? ((t.bid_px + t.ask_px) / 2) : 0;
  }

  // Market Phase：顺序处理该 symbol 在同 ts 的所有 market events
  void process_market_events(const std::vector<q::market::MarketEvent>& evs, std::int64_t ts_ns) {
    fills.clear();
    updates.clear();

    for (const auto& e : evs) {
      builder.on_event(e);
      if (!builder.book_valid()) continue;

      refresh_view(ts_ns);

      // 真实撮合推进（partial fill / cancel effective / ttl 等都在项目二 exec 内）
      auto fs = exec.on_market(oms, last_mv);
      for (auto& f : fs) fills.push_back(f);

      // 异步回报（CancelAck / Filled / PartiallyFilled / Expired...）
      auto ups = exec.drain_updates(); // 如果你项目二方法名不同，这里改一下
      for (auto& u : ups) updates.push_back(std::move(u));
    }
  }

  // Order Phase：执行 cancel / submit（仍然只在 owning worker 线程触发）
  struct SubmitCmd { bt::OrderRequest req; };
  struct CancelCmd { std::int64_t order_id{}; };

  void process_commands(const std::vector<CancelCmd>& cancels,
                        const std::vector<SubmitCmd>& submits) {
    fills.clear();
    updates.clear();

    // 先 cancel
    for (auto const& c : cancels) {
      auto up = exec.cancel(oms, last_mv, c.order_id);
      updates.push_back(std::move(up));
    }

    // 再 submit（注意：submit 可能立即产生 Fill）
    for (auto const& s : submits) {
      auto res = exec.submit(oms, last_mv, s.req);
      updates.push_back(std::move(res.ack));
      if (res.fill) fills.push_back(*res.fill);

      auto ups = exec.drain_updates();
      for (auto& u : ups) updates.push_back(std::move(u));
    }
  }
};

} // namespace bt3
