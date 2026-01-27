#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bt3 {

using SymbolId = std::uint32_t;

struct Instrument {
  SymbolId id{};
  std::string symbol;
};

struct MarketView {
  std::int64_t ts_ns{};
  std::int64_t best_bid_px{};
  std::int64_t best_ask_px{};
  std::int64_t mid_px{};
};

struct TickEvent {
  std::int64_t ts_ns{};
  SymbolId sym{};
  std::int64_t best_bid_px{};
  std::int64_t best_ask_px{};
  std::int64_t mid_px{};
};

} // namespace bt3
