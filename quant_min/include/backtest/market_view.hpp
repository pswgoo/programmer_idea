#pragma once

struct MarketView {
  std::int64_t ts_ns{};
  std::int64_t best_bid_px{};
  std::int64_t best_ask_px{};
  std::int64_t mid_px{};
};
