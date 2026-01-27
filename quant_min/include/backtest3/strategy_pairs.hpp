#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <cmath>

#include "backtest3/multi_market_view.hpp"
#include "backtest3/orders.hpp"
#include "backtest3/portfolio.hpp"

namespace bt3 {

struct PairsConfig {
  std::size_t idx_a{0};
  std::size_t idx_b{1};
  std::size_t window{200};
  double entry_z{2.0};
  double exit_z{0.5};
  std::int64_t qty{10};
};

class PairsMeanReversionStrategy {
public:
  explicit PairsMeanReversionStrategy(PairsConfig cfg) : cfg_(cfg) {}

  std::vector<OrderIntent> on_batch(const MultiMarketView& mmv, const Portfolio& pf, SymbolId sym_a, SymbolId sym_b) {
    std::vector<OrderIntent> out;
    const auto& a = mmv.mv(cfg_.idx_a);
    const auto& b = mmv.mv(cfg_.idx_b);
    if (a.mid_px <= 0 || b.mid_px <= 0) return out;

    const double spread = static_cast<double>(a.mid_px - b.mid_px);
    push(spread);
    if (spreads_.size() < cfg_.window) return out;

    const double mu = mean();
    const double sd = stddev(mu);
    if (sd <= 1e-9) return out;

    const double z = (spread - mu) / sd;

    // 当前持仓（以 qty 为单位的简单对冲）
    const std::int64_t pos_a = pf.pos(cfg_.idx_a);
    const std::int64_t pos_b = pf.pos(cfg_.idx_b);

    // 我们定义“目标持仓”只有三种：0 / (+qty,-qty) / (-qty,+qty)
    // 用 zscore 决定进入/退出
    if (std::fabs(z) < cfg_.exit_z) {
      // exit: flatten
      if (pos_a != 0) {
        out.push_back(OrderIntent{sym_a, pos_a > 0 ? Side::Sell : Side::Buy, std::llabs(pos_a), a.mid_px});
      }
      if (pos_b != 0) {
        out.push_back(OrderIntent{sym_b, pos_b > 0 ? Side::Sell : Side::Buy, std::llabs(pos_b), b.mid_px});
      }
      return out;
    }

    if (z > cfg_.entry_z) {
      // spread too high: short A, long B
      // target: pos_a = -qty, pos_b = +qty
      out = rebalance_to(sym_a, sym_b, a, b, pos_a, pos_b, -cfg_.qty, +cfg_.qty);
      return out;
    }

    if (z < -cfg_.entry_z) {
      // spread too low: long A, short B
      out = rebalance_to(sym_a, sym_b, a, b, pos_a, pos_b, +cfg_.qty, -cfg_.qty);
      return out;
    }

    return out;
  }

private:
  void push(double x) {
    spreads_.push_back(x);
    sum_ += x;
    sumsq_ += x * x;
    if (spreads_.size() > cfg_.window) {
      double y = spreads_.front();
      spreads_.pop_front();
      sum_ -= y;
      sumsq_ -= y * y;
    }
  }

  double mean() const { return sum_ / static_cast<double>(spreads_.size()); }

  double stddev(double mu) const {
    double n = static_cast<double>(spreads_.size());
    double var = (sumsq_ / n) - mu * mu;
    return std::sqrt(std::max(0.0, var));
  }

  std::vector<OrderIntent> rebalance_to(SymbolId sym_a, SymbolId sym_b,
                                        const MarketView& a, const MarketView& b,
                                        std::int64_t pos_a, std::int64_t pos_b,
                                        std::int64_t tgt_a, std::int64_t tgt_b) {
    std::vector<OrderIntent> out;
    const auto da = tgt_a - pos_a;
    const auto db = tgt_b - pos_b;

    if (da != 0) {
      out.push_back(OrderIntent{sym_a, da > 0 ? Side::Buy : Side::Sell, std::llabs(da), a.mid_px});
    }
    if (db != 0) {
      out.push_back(OrderIntent{sym_b, db > 0 ? Side::Buy : Side::Sell, std::llabs(db), b.mid_px});
    }
    return out;
  }

private:
  PairsConfig cfg_;
  std::deque<double> spreads_;
  double sum_{0.0};
  double sumsq_{0.0};
};

} // namespace bt3
