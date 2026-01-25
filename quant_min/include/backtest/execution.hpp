#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "backtest/oms.hpp"
#include "backtest/market_view.hpp"

namespace bt {

struct ExecConfig {
  bool allow_taker_fill{true};
  bool enable_partial_fill{true};
  std::int64_t max_fill_qty_per_tick{1}; // 强制每 tick 只成交 1


  // --- async cancel ---
  // base delay in ns: set to 1ms by default
  std::int64_t cancel_delay_base_ns{1'000'000}; // 1ms
  // jitter range in ns: additional [0, cancel_delay_jitter_ns]
  std::int64_t cancel_delay_jitter_ns{4'000'000}; // +0~4ms => total 1~5ms
  std::uint64_t cancel_delay_seed{0xC0FFEEu};      // deterministic

  // --- order ttl ---
  // 0 = disabled. if >0, orders expire after this duration unless request sets its own TTL (not added yet)
  std::int64_t default_ttl_ns{0};
};

class ExecutionSim {
public:
  explicit ExecutionSim(ExecConfig cfg = {}) : cfg_(cfg) {}

  struct SubmitResult {
    std::int64_t order_id{0};         // NEW: always filled for accepted
    OrderUpdate ack;
    std::optional<FillEvent> fill;    // possible immediate fill (partial/full)
  };

  // drain async updates produced by on_market (e.g., CancelAck, Expire)
  std::vector<OrderUpdate> drain_updates() {
    std::vector<OrderUpdate> out;
    out.swap(pending_updates_);
    return out;
  }

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
    if (cfg_.default_ttl_ns > 0) {
      o.expire_ts_ns = mv.ts_ns + cfg_.default_ttl_ns;
    } else {
      o.expire_ts_ns = 0;
    }
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
       // already requested?
      if (o->status == OrderStatus::CancelRequested) {
        return {mv.ts_ns, order_id, OrderStatus::CancelRequested, ""};
      }

      o->status = OrderStatus::CancelRequested;
      o->cancel_req_ts_ns = mv.ts_ns;
      o->cancel_effective_ts_ns = mv.ts_ns + cancel_delay_ns_det(order_id);
 
       // request accepted (ack of request, NOT final cancel ack)
      return {mv.ts_ns, order_id, OrderStatus::CancelRequested, ""};
    }

    return {mv.ts_ns, order_id, OrderStatus::Rejected, "not_cancelable"};
  }

  // 每个 market tick 扫描 active orders，触发成交（partial/full）
  std::vector<FillEvent> on_market(Oms& oms, const MarketView& mv) {
    std::vector<FillEvent> fills;
    if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) return fills;

    const auto now = mv.ts_ns;

    // ---- phase 0: TTL expiry -> CancelRequested ----
    if (cfg_.default_ttl_ns > 0) {
      for (auto* o : oms.active_orders()) {
        if (!o) continue;
        if (o->status != OrderStatus::Working && o->status != OrderStatus::PartiallyFilled) continue;
        if (o->leaves_qty <= 0) continue;

        if (o->expire_ts_ns > 0 && now >= o->expire_ts_ns) {
          // convert to CancelRequested if not already
          o->status = OrderStatus::CancelRequested;
          o->cancel_req_ts_ns = now;
          o->cancel_effective_ts_ns = now + cancel_delay_ns_det(o->order_id);

          // optional: emit an update to indicate expiry requested
          pending_updates_.push_back(OrderUpdate{now, o->order_id, OrderStatus::CancelRequested, "expired"});
        }
      }
    }

    // ---- phase 1: cancel effective -> Canceled (CancelAck) ----
    for (auto* o : oms.active_orders()) {
      if (!o) continue;
      if (o->status != OrderStatus::CancelRequested) continue;

      // if effective_ts not set (defensive)
      if (o->cancel_effective_ts_ns == 0) {
        o->cancel_effective_ts_ns = now + cancel_delay_ns_det(o->order_id);
      }

      if (now >= o->cancel_effective_ts_ns) {
        o->status = OrderStatus::Canceled;
        o->leaves_qty = 0;
        pending_updates_.push_back(OrderUpdate{now, o->order_id, OrderStatus::Canceled, ""});
      }
    }

    // ---- phase 2: matching ----
    for (auto* o : oms.active_orders()) {
      if (!o) continue;

      // only fill if still has leaves
      if (o->leaves_qty <= 0) continue;

      // do not fill canceled/filled/rejected
      if (o->status == OrderStatus::Canceled || o->status == OrderStatus::Filled || o->status == OrderStatus::Rejected) continue;

      // allow matching for Working / PartiallyFilled / CancelRequested (before effective cancel)
      if (o->status != OrderStatus::Working &&
          o->status != OrderStatus::PartiallyFilled &&
          o->status != OrderStatus::CancelRequested) {
        continue;
      }

      if (can_fill_now(*o, mv)) {
        const auto fill_px = (o->side == Side::Buy) ? mv.best_ask_px : mv.best_bid_px;
        fills.push_back(do_fill(*o, now, fill_px));
        // If order filled, emit update (optional but useful)
        if (o->status == OrderStatus::Filled) {
          pending_updates_.push_back(OrderUpdate{now, o->order_id, OrderStatus::Filled, ""});
        }
      }
    }

    return fills;
  }


private:
  static std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  std::int64_t cancel_delay_ns_det(std::int64_t order_id) const {
    // delay = base + (hash % (jitter+1))
    const auto base = std::max<std::int64_t>(0, cfg_.cancel_delay_base_ns);
    const auto jit  = std::max<std::int64_t>(0, cfg_.cancel_delay_jitter_ns);
    if (jit == 0) return base;

    std::uint64_t x = static_cast<std::uint64_t>(order_id) ^ cfg_.cancel_delay_seed;
    std::uint64_t h = splitmix64(x);
    std::int64_t add = static_cast<std::int64_t>(h % static_cast<std::uint64_t>(jit + 1));
    return base + add; // => [base, base+jit]
  }
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
  std::vector<OrderUpdate> pending_updates_;
};

} // namespace bt
