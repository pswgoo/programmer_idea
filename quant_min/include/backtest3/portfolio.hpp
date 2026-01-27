#pragma once
#include <cstdint>
#include <vector>

namespace bt3 {

struct PositionState {
  std::int64_t pos{0};
  std::int64_t cash{0};
};

class Portfolio {
public:
  explicit Portfolio(std::size_t n_syms = 0) : pos_(n_syms) {}

  void resize(std::size_t n_syms) { pos_.assign(n_syms, PositionState{}); }

  std::int64_t position_idx(std::size_t idx) const { return pos_[idx].pos; }

  void set_position_idx(std::size_t idx, std::int64_t p) { pos_[idx].pos = p; }

  double equity(const std::vector<bt3::MarketView>& mvs) const {
    long double eq = 0.0L;
    const auto n = pos_.size();
    for (std::size_t i = 0; i < n; ++i) {
      eq += static_cast<long double>(pos_[i].cash);
      eq += static_cast<long double>(pos_[i].pos) * static_cast<long double>(mvs[i].mid_px);
    }
    return static_cast<double>(eq);
  }

private:
  std::vector<PositionState> pos_;
};

} // namespace bt3
