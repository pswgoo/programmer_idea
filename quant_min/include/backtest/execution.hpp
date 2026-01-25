#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "backtest/oms.hpp"

// 你 backtest 里已有 MarketView
struct MarketView {
  std::int64_t ts_ns{};
  std::int64_t best_bid_px{};
  std::int64_t best_ask_px{};
  std::int64_t mid_px{};
};

namespace bt {

struct ExecConfig {
  bool allow_taker_fill{true};
  bool enable_partial_fill{true};
  std::int64_t max_fill_qty_per_tick{1}; // 强制每 tick 只成交 1
};

class ExecutionSim {
public:
  explicit ExecutionSim(ExecConfig cfg = {}) : cfg_(cfg) {}

  struct SubmitResult {
    std::int64_t order_id{0};         // NEW: always filled for accepted
    OrderUpdate ack;
    std::optional<FillEvent> fill;    // possible immediate fill (partial/full)
  };

  SubmitResult submit(Oms& oms, const MarketView& mv, const OrderRequest& req) {
    SubmitResult r;

    // validate
    if (req.qty <= 0) {
      r.ack = {mv.ts_ns, 0, OrderStatus::Rejected, "qty<=0"};
      return r;
    }
    if (req.type == OrderType::Limit && req.limit_px <= 0) {
      r.ack = {mv.ts_ns, 0, OrderStatus::Rejected, "limit_px<=0"};
      return r;
    }
    if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) {
      r.ack = {mv.ts_ns, 0, OrderStatus::Rejected, "no_market"};
      return r;
    }

    Order& o = oms.add_new(mv.ts_ns, req);
    r.order_id = o.order_id;

    // accept
    o.status = OrderStatus::Working;
    r.ack = {mv.ts_ns, o.order_id, OrderStatus::Working, ""};

    // immediate taker fill if crosses
    if (cfg_.allow_taker_fill && can_fill_now(o, mv) && o.leaves_qty > 0) {
      const auto px = (o.side == Side::Buy) ? mv.best_ask_px : mv.best_bid_px;
      r.fill = do_fill(o, mv.ts_ns, px);
    }

    return r;
  }

  // Cancel: allow cancel Working/PartiallyFilled
  OrderUpdate cancel(Oms& oms, const MarketView& mv, std::int64_t order_id) {
    auto* o = oms.get(order_id);
    if (!o) return {mv.ts_ns, order_id, OrderStatus::Rejected, "unknown_order"};

    if (o->status == OrderStatus::Working || o->status == OrderStatus::PartiallyFilled) {
      o->status = OrderStatus::Canceled;
      o->leaves_qty = 0;
      return {mv.ts_ns, order_id, OrderStatus::Canceled, ""};
    }

    return {mv.ts_ns, order_id, OrderStatus::Rejected, "not_cancelable"};
  }

  // 每个 market tick 扫描 active orders，触发成交（partial/full）
  std::vector<FillEvent> on_market(Oms& oms, const MarketView& mv) {
    std::vector<FillEvent> fills;
    if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) return fills;

    for (auto* o : oms.active_orders()) {
      if (!o) continue;

      if (o->status == OrderStatus::CancelRequested) {
        o->status = OrderStatus::Canceled;
        o->leaves_qty = 0;
        continue;
      }

      if (o->status != OrderStatus::Working && o->status != OrderStatus::PartiallyFilled) continue;
      if (o->leaves_qty <= 0) continue;

      if (can_fill_now(*o, mv)) {
        const auto px = (o->side == Side::Buy) ? mv.best_ask_px : mv.best_bid_px;
        fills.push_back(do_fill(*o, mv.ts_ns, px));
      }
    }
    return fills;
  }

private:
  static bool can_fill_now(const Order& o, const MarketView& mv) {
    if (o.side == Side::Buy) return o.limit_px >= mv.best_ask_px;
    return o.limit_px <= mv.best_bid_px;
  }

  FillEvent do_fill(Order& o, std::int64_t ts_ns, std::int64_t px) {
    std::int64_t fill_qty = o.leaves_qty;
    if (cfg_.enable_partial_fill) {
      const std::int64_t cap = std::max<std::int64_t>(1, cfg_.max_fill_qty_per_tick);
      fill_qty = std::min<std::int64_t>(o.leaves_qty, cap);
    }

    o.leaves_qty -= fill_qty;
    if (o.leaves_qty == 0) o.status = OrderStatus::Filled;
    else o.status = OrderStatus::PartiallyFilled;

    return FillEvent{ts_ns, o.order_id, o.side, px, fill_qty};
  }

private:
  ExecConfig cfg_;
};

} // namespace bt
