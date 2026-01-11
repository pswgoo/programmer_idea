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
  void on_tick(const q::market::Tick& t) {
    auto& side_map = (t.side == q::market::Side::Bid) ? bids_ : asks_;
    // 这里把 tick 当作“该价位的增量量”来演示（真实行情一般有更复杂的增量/快照逻辑）
    side_map[t.price] += t.qty;
    if (side_map[t.price] <= 0) side_map.erase(t.price);
  }

  struct Top {
    std::int64_t bid_px{0}, bid_qty{0};
    std::int64_t ask_px{0}, ask_qty{0};
    bool valid{false};
  };

  Top top() const {
    Top t{};
    if (bids_.empty() || asks_.empty()) return t;

    // bids: highest price
    auto itb = bids_.rbegin();
    t.bid_px = itb->first;
    t.bid_qty = itb->second;

    // asks: lowest price
    auto ita = asks_.begin();
    t.ask_px = ita->first;
    t.ask_qty = ita->second;

    t.valid = true;
    return t;
  }

private:
  // price -> qty
  std::map<std::int64_t, std::int64_t> bids_;
  std::map<std::int64_t, std::int64_t> asks_;
};

} // namespace q::book
