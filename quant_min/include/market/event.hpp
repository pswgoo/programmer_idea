#pragma once
#include <cstdint>

namespace q::market {

enum class Side : std::uint8_t { Bid = 0, Ask = 1, Unknown = 2 };

enum class Kind : std::uint8_t {
  SnapshotBegin,   // SB
  SnapshotLevel,   // SL
  SnapshotEnd,     // SE
  Incremental      // I
};

enum class Action : std::uint8_t { New, Change, Delete, None };

// 真实行情事件：带 seq + 快照/增量语义
struct MarketEvent {
  std::int64_t ts_ns{};
  std::int64_t seq{};

  Kind kind{Kind::Incremental};

  Side side{Side::Unknown};
  std::int64_t price{};
  std::int64_t qty{};

  Action action{Action::None};
};

} // namespace q::market
