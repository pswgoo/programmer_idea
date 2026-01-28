#pragma once
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "backtest/market_view.hpp"   // MarketView
#include "backtest/orders.hpp"      // bt::OrderRequest / Side / OrderStatus
#include "backtest/oms.hpp"         // bt::Oms
#include "backtest3/portfolio.hpp"  // bt3::Portfolio

namespace bt3 {

struct CancelIntent {
  std::size_t sym_idx{};
  std::int64_t order_id{};
};

struct OrderIntent {
  std::size_t sym_idx{};
  bt::OrderRequest req;
};

struct PortfolioDecision {
  std::vector<CancelIntent> cancels;
  std::vector<OrderIntent> submits;
};

// 每个品种一份状态：rolling mean + working order ids
struct PerSymState {
  std::deque<std::int64_t> mids;
  long double sum{0.0L};

  std::int64_t working_buy_id{0};
  std::int64_t working_sell_id{0};

  // 用于“撤单重挂”：多久没改善/没成交就撤
  std::int64_t last_quote_ts_ns{0};
};

struct MeanRevPortfolioConfig {
  std::size_t window{200};
  double threshold{0.001};             // 0.1%
  std::int64_t trade_qty{10};

  // 撤单重挂阈值（逻辑时间）
  std::int64_t reprice_after_ns{5'000'000}; // 5ms

  // 允许同时挂多笔？（默认每侧最多1笔）
  bool one_order_per_side{true};
};

class MeanReversionPortfolioStrategy {
public:
  MeanReversionPortfolioStrategy(std::size_t n_syms, MeanRevPortfolioConfig cfg)
      : st_(n_syms), cfg_(cfg) {}

  // barrier 后：引擎把 OMS 回报喂给策略
  void on_order_updated(std::size_t sym_idx, const bt::Oms& oms, const bt::OrderUpdate& up) {
    (void)oms;
    auto& s = st_[sym_idx];

    auto clear_if_match = [&](std::int64_t& slot) {
      if (slot == up.order_id) slot = 0;
    };

    switch (up.status) {
      case bt::OrderStatus::Canceled:
      case bt::OrderStatus::Rejected:
      case bt::OrderStatus::Filled:
        clear_if_match(s.working_buy_id);
        clear_if_match(s.working_sell_id);
        break;
      case bt::OrderStatus::Working:
      case bt::OrderStatus::PartiallyFilled:
      case bt::OrderStatus::CancelRequested:
      default:
        break;
    }
  }

  // 可选：submit ack 时登记 working id（比等 on_order_updated 更快更稳）
  void on_submit_ack(std::size_t sym_idx, const bt::OrderUpdate& ack, bt::Side side) {
    if (ack.status != bt::OrderStatus::Working && ack.status != bt::OrderStatus::PartiallyFilled) return;
    auto& s = st_[sym_idx];
    if (side == bt::Side::Buy) s.working_buy_id = ack.order_id;
    else s.working_sell_id = ack.order_id;
  }

  PortfolioDecision on_batch(const std::vector<MarketView>& mvs, const Portfolio& pf) {
    PortfolioDecision dec;
    dec.cancels.reserve(64);
    dec.submits.reserve(64);

    for (std::size_t i = 0; i < mvs.size(); ++i) {
      const auto& mv = mvs[i];
      if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) continue;

      auto& s = st_[i];
      push_mid(s, mv.mid_px);

      if (s.mids.size() < cfg_.window) continue;

      const double ma = mean_mid(s);
      const double mid = static_cast<double>(mv.mid_px);
      const double upper = ma * (1.0 + cfg_.threshold);
      const double lower = ma * (1.0 - cfg_.threshold);

      const std::int64_t pos = pf.pos(i);

      // 1) reprice: 若挂单太久没变化/没成交，发 cancel（异步）
      if (cfg_.reprice_after_ns > 0) {
        const auto too_old = (s.last_quote_ts_ns > 0) && ((mv.ts_ns - s.last_quote_ts_ns) >= cfg_.reprice_after_ns);

        if (too_old) {
          if (s.working_buy_id != 0) {
            dec.cancels.push_back(CancelIntent{i, s.working_buy_id});
            // 不立即清 id，等 Canceled 回报再清，保持真实
          }
          if (s.working_sell_id != 0) {
            dec.cancels.push_back(CancelIntent{i, s.working_sell_id});
          }
          // 避免每个 batch 都 cancel spam
          s.last_quote_ts_ns = mv.ts_ns;
        }
      }

      // 2) signal: 只做多均值回归（与你项目二一致）
      // 低于 lower：开多；高于 upper：平仓
      if (pos == 0 && can_place_buy(s)) {
        if (mid < lower) {
          bt::OrderRequest req;
          req.type = bt::OrderType::Limit;
          req.side = bt::Side::Buy;
          req.qty = cfg_.trade_qty;
          req.limit_px = mv.best_bid_px; // maker buy
          dec.submits.push_back(OrderIntent{i, req});
          s.last_quote_ts_ns = mv.ts_ns;
        }
      }

      if (pos > 0 && can_place_sell(s)) {
        if (mid > upper) {
          bt::OrderRequest req;
          req.type = bt::OrderType::Limit;
          req.side = bt::Side::Sell;
          req.qty = pos; // 全平
          req.limit_px = mv.best_ask_px; // maker sell
          dec.submits.push_back(OrderIntent{i, req});
          s.last_quote_ts_ns = mv.ts_ns;
        }
      }
    }

    return dec;
  }

private:
  static void push_mid(PerSymState& s, std::int64_t mid) {
    s.mids.push_back(mid);
    s.sum += static_cast<long double>(mid);
    if (s.mids.size() > 4000) { // 安全上限
      s.sum -= static_cast<long double>(s.mids.front());
      s.mids.pop_front();
    }
  }

  double mean_mid(const PerSymState& s) const {
    const auto n = std::min<std::size_t>(s.mids.size(), cfg_.window);
    long double sum = 0.0L;
    for (std::size_t k = s.mids.size() - n; k < s.mids.size(); ++k) sum += s.mids[k];
    return static_cast<double>(sum / static_cast<long double>(n));
  }

  bool can_place_buy(const PerSymState& s) const {
    if (!cfg_.one_order_per_side) return true;
    return s.working_buy_id == 0;
  }
  bool can_place_sell(const PerSymState& s) const {
    if (!cfg_.one_order_per_side) return true;
    return s.working_sell_id == 0;
  }

private:
  std::vector<PerSymState> st_;
  MeanRevPortfolioConfig cfg_;
};

} // namespace bt3
