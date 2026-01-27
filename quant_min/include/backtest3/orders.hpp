#pragma once
#include <cstdint>

#include "backtest3/types.hpp"

namespace bt3 {

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };

struct OrderIntent {
  SymbolId sym{};
  Side side{Side::Buy};
  std::int64_t qty{0};
  std::int64_t limit_px{0}; // Milestone 3 简化：limit=mid
};

struct Fill {
  std::int64_t ts_ns{};
  SymbolId sym{};
  Side side{Side::Buy};
  std::int64_t price{0};
  std::int64_t qty{0};
};

} // namespace bt3
