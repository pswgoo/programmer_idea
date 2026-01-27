#pragma once
#include <cstdint>
#include <cmath>
#include <string>

#include "backtest3/orders.hpp"
#include "backtest3/portfolio.hpp"
#include "backtest3/types.hpp"

namespace bt3 {

struct PortfolioRiskConfig {
  std::int64_t max_gross_notional{1'000'000}; // sum(|pos|*mid)
  std::int64_t max_abs_position_per_sym{1000};
  std::int64_t max_order_qty{200};

  bool enable_kill_switch{true};
  std::int64_t max_consecutive_rejects{50};

  // 回撤熔断（可选）
  bool enable_drawdown_kill{false};
  double max_drawdown{0.2}; // 20%
};

class PortfolioRisk {
public:
  explicit PortfolioRisk(PortfolioRiskConfig cfg = {}) : cfg_(cfg) {}

  struct Decision {
    bool ok{true};
    std::string reason;
  };

  void on_equity(double eq) {
    if (!cfg_.enable_drawdown_kill) return;
    if (eq_peak_ < 0) eq_peak_ = eq;
    if (eq > eq_peak_) eq_peak_ = eq;
    if (eq_peak_ > 0) {
      double dd = (eq_peak_ - eq) / eq_peak_;
      if (dd >= cfg_.max_drawdown) killed_ = true;
    }
  }

  bool killed() const { return killed_; }

  Decision pre_trade_check(const Portfolio& pf,
                           const std::vector<MarketView>& mvs,
                           std::size_t sym_idx,
                           const OrderIntent& oi) {
    if (killed_) return reject("killed");
    if (oi.qty <= 0) return reject("qty<=0");
    if (oi.qty > cfg_.max_order_qty) return reject("qty>max_order_qty");

    const auto mid = mvs[sym_idx].mid_px;
    if (mid <= 0) return reject("no_mid");

    // per-symbol position limit (worst-case)
    std::int64_t next_pos = pf.pos(sym_idx) + ((oi.side == Side::Buy) ? oi.qty : -oi.qty);
    if (std::llabs(next_pos) > cfg_.max_abs_position_per_sym) return reject("pos_limit_sym");

    // gross notional after trade (worst-case, simplistic)
    // compute gross = sum(|pos|*mid) with hypothetical next_pos for this sym
    long double gross = 0.0L;
    for (std::size_t i = 0; i < pf.n_syms(); ++i) {
      std::int64_t p = pf.pos(i);
      if (i == sym_idx) p = next_pos;
      gross += static_cast<long double>(std::llabs(p)) * static_cast<long double>(mvs[i].mid_px);
    }
    if (gross > static_cast<long double>(cfg_.max_gross_notional)) return reject("gross_notional");

    // pass
    consecutive_rejects_ = 0;
    last_reason_.clear();
    return {true, ""};
  }

private:
  Decision reject(const char* r) {
    last_reason_ = r;
    ++consecutive_rejects_;
    if (cfg_.enable_kill_switch && consecutive_rejects_ >= cfg_.max_consecutive_rejects) {
      killed_ = true;
    }
    return {false, r};
  }

private:
  PortfolioRiskConfig cfg_;
  bool killed_{false};
  std::int64_t consecutive_rejects_{0};
  std::string last_reason_;

  double eq_peak_{-1.0};
};

} // namespace bt3
