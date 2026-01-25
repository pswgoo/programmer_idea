#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "backtest/orders.hpp"

namespace bt {

class Oms {
public:
  std::int64_t next_id() { return ++last_id_; }

  Order& add_new(std::int64_t ts_ns, const OrderRequest& req) {
    Order o;
    o.order_id = next_id();
    o.side = req.side;
    o.qty = req.qty;
    o.leaves_qty = req.qty;
    o.limit_px = req.limit_px;
    o.status = OrderStatus::PendingNew;
    o.create_ts_ns = ts_ns;
    auto [it, ok] = orders_.emplace(o.order_id, std::move(o));
    return it->second;
  }

  Order* get(std::int64_t id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return nullptr;
    return &it->second;
  }

  const Order* get(std::int64_t id) const {
    auto it = orders_.find(id);
    if (it == orders_.end()) return nullptr;
    return &it->second;
  }

  // Active = still eligible for execution processing
  // i.e. Working / PartiallyFilled / CancelRequested
  std::vector<Order*> active_orders() {
    std::vector<Order*> out;
    out.reserve(orders_.size());
    for (auto& kv : orders_) {
      auto& o = kv.second;
      if (o.status == OrderStatus::Working ||
          o.status == OrderStatus::PartiallyFilled ||
          o.status == OrderStatus::CancelRequested) {
        out.push_back(&o);
      }
    }
    return out;
  }

  bool has_working(Side side) const {
    for (auto const& kv : orders_) {
      auto const& o = kv.second;
      if (o.side != side) continue;
      if (o.status == OrderStatus::Working || o.status == OrderStatus::PartiallyFilled) return true;
    }
    return false;
  }

  std::vector<std::int64_t> working_order_ids_by_side(Side side) const {
    std::vector<std::int64_t> ids;
    for (auto const& kv : orders_) {
      auto const& o = kv.second;
      if (o.side != side) continue;
      if (o.status == OrderStatus::Working || o.status == OrderStatus::PartiallyFilled) {
        ids.push_back(o.order_id);
      }
    }
    return ids;
  }

private:
  std::int64_t last_id_{0};
  std::unordered_map<std::int64_t, Order> orders_;
};

} // namespace bt
