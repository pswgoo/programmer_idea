#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

#include "market/event.hpp"

namespace q::book {

// 中低频最小实现：用 map 聚合价位（后续可替换 flat_map / 数组）
class L2Book {
public:
  void clear() {
    bids_.clear();
    asks_.clear();
  }

  void apply_snapshot_level(q::market::Side side, std::int64_t price, std::int64_t qty) {
    auto& m = (side == q::market::Side::Bid) ? bids_ : asks_;
    if (qty <= 0) m.erase(price);
    else m[price] = qty;
  }

  bool apply_incremental(q::market::Side side, std::int64_t price, std::int64_t qty, q::market::Action action) {
    bool ok = true;
    if (side != q::market::Side::Bid && side != q::market::Side::Ask) return false;

    auto& m = (side == q::market::Side::Bid) ? bids_ : asks_;
    auto it = m.find(price);
    const bool exists = (it != m.end());

    switch (action) {
      case q::market::Action::New:
        if (exists) ok = false;
        if (qty > 0) m[price] = qty;
        else ok = false;
        break;
      case q::market::Action::Change:
        if (!exists) ok = false;
        if (qty <= 0) {
          if (exists) m.erase(it);
        } else {
          m[price] = qty;
        }
        break;
      case q::market::Action::Delete:
        if (!exists) ok = false;
        if (exists) m.erase(it);
        break;
      default:
        return false;
    }
    return ok;
  }

  struct Top {
    std::int64_t bid_px{0}, bid_qty{0};
    std::int64_t ask_px{0}, ask_qty{0};
    bool valid{false};
  };

  Top top() const {
    Top t{};
    if (bids_.empty() || asks_.empty()) return t;

    auto itb = bids_.rbegin();     // highest bid
    auto ita = asks_.begin();      // lowest ask
    t.bid_px = itb->first; t.bid_qty = itb->second;
    t.ask_px = ita->first; t.ask_qty = ita->second;
    t.valid = true;
    return t;
  }

private:
  std::map<std::int64_t, std::int64_t> bids_;
  std::map<std::int64_t, std::int64_t> asks_;
};

} // namespace q::book
