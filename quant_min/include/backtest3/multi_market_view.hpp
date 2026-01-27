#pragma once
#include <cstddef>
#include <vector>

#include "backtest3/types.hpp"

namespace bt3 {

struct MultiMarketView {
  std::int64_t ts_ns{};
  const std::vector<MarketView>* mvs{nullptr}; // snapshot after barrier
  const MarketView& mv(std::size_t idx) const { return (*mvs)[idx]; }
};

} // namespace bt3
