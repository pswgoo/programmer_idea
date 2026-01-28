#pragma once
#include <cstdint>
#include <vector>
#include <cmath>

#include "backtest3/types.hpp"
#include "backtest/orders.hpp"

namespace bt3 {

struct PositionState {
  std::int64_t pos{0};
  std::int64_t cash{0}; // 用“价格单位 * qty”的整数现金（和 mid 一致）
};

class Portfolio {
public:
  explicit Portfolio(std::size_t n_syms = 0) : st_(n_syms) {}

  void resize(std::size_t n_syms) { st_.assign(n_syms, PositionState{}); }

  std::int64_t pos(std::size_t idx) const { return st_[idx].pos; }
  std::int64_t cash(std::size_t idx) const { return st_[idx].cash; }

  void set_pos(std::size_t idx, std::int64_t p) { st_[idx].pos = p; }
  void set_cash(std::size_t idx, std::int64_t c) { st_[idx].cash = c; }

  void apply_fill(std::size_t idx, const bt::FillEvent& f) {
    // 买入：pos+qty，cash -= px*qty
    // 卖出：pos-qty，cash += px*qty
    const std::int64_t notional = f.price * f.qty;
    if (f.side == bt::Side::Buy) {
      st_[idx].pos += f.qty;
      st_[idx].cash -= notional;
    } else {
      st_[idx].pos -= f.qty;
      st_[idx].cash += notional;
    }
  }

  double equity(const std::vector<MarketView>& mvs) const {
    long double eq = 0.0L;
    for (std::size_t i = 0; i < st_.size(); ++i) {
      eq += static_cast<long double>(st_[i].cash);
      eq += static_cast<long double>(st_[i].pos) * static_cast<long double>(mvs[i].mid_px);
    }
    return static_cast<double>(eq);
  }

  std::size_t n_syms() const { return st_.size(); }

private:
  std::vector<PositionState> st_;
};

} // namespace bt3
