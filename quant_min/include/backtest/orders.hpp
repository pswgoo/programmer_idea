#pragma once
#include <cstdint>
#include <string>

namespace bt {

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : std::uint8_t { Limit = 0 };
enum class TimeInForce : std::uint8_t { GTC = 0 };

enum class OrderStatus : std::uint8_t {
  PendingNew,
  Working,
  PartiallyFilled,
  Filled,
  CancelRequested,
  Canceled,
  Rejected
};

struct OrderRequest {
  OrderType type{OrderType::Limit};
  Side side{Side::Buy};
  std::int64_t qty{0};
  std::int64_t limit_px{0}; // for Limit
  TimeInForce tif{TimeInForce::GTC};
};

struct CancelRequest {
  std::int64_t order_id{0};
};

struct Order {
  std::int64_t order_id{0};
  Side side{Side::Buy};
  std::int64_t qty{0};
  std::int64_t leaves_qty{0};
  std::int64_t limit_px{0};
  OrderStatus status{OrderStatus::PendingNew};
  std::int64_t create_ts_ns{0};
};

struct OrderUpdate {
  std::int64_t ts_ns{0};
  std::int64_t order_id{0};
  OrderStatus status{OrderStatus::Rejected};
  std::string reason;
};

struct FillEvent {
  std::int64_t ts_ns{0};
  std::int64_t order_id{0};
  Side side{Side::Buy};
  std::int64_t price{0};
  std::int64_t qty{0};
};

} // namespace bt
