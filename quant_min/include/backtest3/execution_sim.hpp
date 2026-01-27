#pragma once
#include <cstdint>
#include <optional>

#include "backtest3/orders.hpp"
#include "backtest3/types.hpp"

namespace bt3 {

struct ExecSimConfig {
  std::int64_t fee_per_unit{0};     // 每股/每张手续费（整数）
  std::int64_t slippage_ticks{0};   // 简化滑点：买在 mid+ticks，卖在 mid-ticks
};

class ExecutionSim {
public:
  explicit ExecutionSim(ExecSimConfig cfg = {}) : cfg_(cfg) {}

  // Milestone 3：一律“立刻成交”在一个确定性价格（mid +/- slippage）
  std::optional<Fill> try_execute(std::int64_t ts_ns,
                                  const MarketView& mv,
                                  const OrderIntent& oi) const {
    if (mv.mid_px <= 0 || oi.qty <= 0) return std::nullopt;

    std::int64_t px = mv.mid_px;
    if (cfg_.slippage_ticks != 0) {
      if (oi.side == Side::Buy) px += cfg_.slippage_ticks;
      else px -= cfg_.slippage_ticks;
    }
    return Fill{ts_ns, oi.sym, oi.side, px, oi.qty};
  }

  std::int64_t fee_for(const Fill& f) const {
    return cfg_.fee_per_unit * f.qty;
  }

private:
  ExecSimConfig cfg_;
};

} // namespace bt3
