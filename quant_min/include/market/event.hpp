#pragma once
#include <cstdint>

namespace q::market {

enum class Side : std::uint8_t { Bid = 0, Ask = 1 };

// 最小 Tick：timestamp(ns), side, price, qty
struct Tick {
  std::int64_t ts_ns{};
  Side side{};
  std::int64_t price{};  // 建议用整数（如 price * 10000）
  std::int64_t qty{};
};

} // namespace q::market
