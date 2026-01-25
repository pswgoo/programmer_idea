#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <optional>
#include <string>

#include "book/book_builder.hpp"
#include "book/flat_l2_book.hpp"
#include "common/clock.hpp"
#include "common/latency.hpp"
#include "common/log.hpp"
#include "market/replay.hpp"
#include "market/event.hpp"
#include "backtest/orders.hpp"
#include "backtest/execution.hpp"
#include "backtest/oms.hpp"

using namespace bt;

struct StratDecision {
  std::optional<bt::OrderRequest> submit;
  bool cancel_buy{false};
  bool cancel_sell{false};
};

// -------------------- Strategy (Mean Reversion) --------------------
// 逻辑：用 mid 的滑动均值 MA；若 mid > MA*(1+th) 且有仓位/允许做空 -> Sell；
//      若 mid < MA*(1-th) -> Buy
// 这里给出“只做多”的最小版：
// - 低于均线阈值：开多
// - 高于均线阈值：平多
class MeanReversionStrategy {
public:
  MeanReversionStrategy(std::size_t window,
                        double threshold,
                        std::int64_t trade_qty)
      : window_(window), threshold_(threshold), trade_qty_(trade_qty) {}

  StratDecision on_market(const MarketView& mv, std::int64_t position) {
    StratDecision d;
    if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) return d;

    push_mid(mv.mid_px);
    if (mids_.size() < window_) return d;

    const double ma = mean_mid();
    const double mid = static_cast<double>(mv.mid_px);

    const double upper = ma * (1.0 + threshold_);
    const double lower = ma * (1.0 - threshold_);

    const bool buy_signal  = (position == 0 && mid < lower);
    const bool sell_signal = (position > 0 && mid > upper);

    // 只做多：开多/平多
    if (position == 0) {
      // 不应留卖单
      if (working_sell_id_ != 0) d.cancel_sell = true;

      if (buy_signal) {
        if (working_buy_id_ == 0) {
          d.submit = bt::OrderRequest{bt::OrderType::Limit, bt::Side::Buy, trade_qty_, mv.best_bid_px, bt::TimeInForce::GTC};
        }
      } else {
        if (working_buy_id_ != 0) d.cancel_buy = true;
      }
    } else { // position > 0
      // 不应留买单（避免加仓）
      if (working_buy_id_ != 0) d.cancel_buy = true;

      if (sell_signal) {
        if (working_sell_id_ == 0) {
          d.submit = bt::OrderRequest{bt::OrderType::Limit, bt::Side::Sell, position, mv.best_ask_px, bt::TimeInForce::GTC};
        }
      } else {
        if (working_sell_id_ != 0) d.cancel_sell = true;
      }
    }

    return d;
  }

  // 引擎在 submit 后调用：把 order_id 写入对应方向
  void on_submit_accepted(bt::Side side, std::int64_t order_id) {
    if (order_id == 0) return;
    if (side == bt::Side::Buy) working_buy_id_ = order_id;
    else working_sell_id_ = order_id;
  }

  // 引擎在 cancel ack 后调用
  void on_canceled(std::int64_t order_id) {
    if (order_id == working_buy_id_) working_buy_id_ = 0;
    if (order_id == working_sell_id_) working_sell_id_ = 0;
  }

  // 引擎在 fill 后调用（partial/full）
  void on_fill(const bt::FillEvent& f, std::int64_t leaves_after) {
    // 如果订单已完全成交，清理对应 id
    if (leaves_after == 0) {
      if (f.order_id == working_buy_id_) working_buy_id_ = 0;
      if (f.order_id == working_sell_id_) working_sell_id_ = 0;
    }
  }

  std::int64_t working_buy_id() const { return working_buy_id_; }
  std::int64_t working_sell_id() const { return working_sell_id_; }

private:
  void push_mid(std::int64_t mid) {
    mids_.push_back(mid);
    sum_ += static_cast<long double>(mid);
    if (mids_.size() > window_) {
      sum_ -= static_cast<long double>(mids_.front());
      mids_.pop_front();
    }
  }

  double mean_mid() const {
    return static_cast<double>(sum_ / static_cast<long double>(mids_.size()));
  }

private:
  std::size_t window_{200};
  double threshold_{0.001};
  std::int64_t trade_qty_{1};

  std::deque<std::int64_t> mids_;
  long double sum_{0.0L};

  std::int64_t working_buy_id_{0};
  std::int64_t working_sell_id_{0};
};

// -------------------- Portfolio --------------------
struct Portfolio {
  long double cash{0.0L};
  std::int64_t position{0};

  void on_fill(const FillEvent& f) {
    const long double px = static_cast<long double>(f.price);
    const long double qty = static_cast<long double>(f.qty);

    if (f.side == Side::Buy) {
      cash -= px * qty;
      position += f.qty;
    } else {
      cash += px * qty;
      position -= f.qty;
    }
  }

  long double equity(std::int64_t mid_px) const {
    return cash + static_cast<long double>(position) * static_cast<long double>(mid_px);
  }
};

// -------------------- Metrics --------------------
struct Metrics {
  // equity curve
  std::vector<long double> equity;
  std::vector<long double> returns; // simple returns per step

  void add_equity(long double eq) {
    if (!equity.empty()) {
      const long double prev = equity.back();
      if (prev != 0.0L) {
        returns.push_back((eq - prev) / prev);
      }
    }
    equity.push_back(eq);
  }

  long double total_return() const {
    if (equity.size() < 2) return 0.0L;
    const long double a = equity.front();
    const long double b = equity.back();
    if (a == 0.0L) return 0.0L;
    return (b - a) / a;
  }

  long double max_drawdown() const {
    if (equity.empty()) return 0.0L;
    long double peak = equity.front();
    long double maxdd = 0.0L;
    for (auto e : equity) {
      if (e > peak) peak = e;
      if (peak > 0.0L) {
        const long double dd = (peak - e) / peak;
        if (dd > maxdd) maxdd = dd;
      }
    }
    return maxdd;
  }

  // 简化 Sharpe：对 step returns 计算 mean/std，年化先不做（你后面可以按 bar 频率年化）
  double sharpe() const {
    if (returns.size() < 2) return 0.0;
    long double sum = 0.0L;
    for (auto r : returns) sum += r;
    const long double mean = sum / static_cast<long double>(returns.size());

    long double var = 0.0L;
    for (auto r : returns) {
      const long double d = r - mean;
      var += d * d;
    }
    var /= static_cast<long double>(returns.size() - 1);
    const long double sd = std::sqrt(static_cast<double>(var));
    if (sd == 0.0L) return 0.0;
    return static_cast<double>(mean / sd);
  }
};

// -------------------- CLI --------------------
static void usage() {
  std::cout
    << "Usage:\n"
    << "  ./backtest_min --file <md.csv> [--speed 0] [--sample 0|K]\n"
    << "                [--cash C] [--window W] [--th T] [--qty Q]\n\n"
    << "Notes:\n"
    << "  Input CSV header:\n"
    << "    ts_ns,seq,kind,side,price,qty,action\n"
    << "  kind: SB,SL,SE,I  action: N,C,D (only for I)\n";
}

int main(int argc, char** argv) {
  std::string file = "data/sample_md_1m.csv";
  double speed = 0.0;
  std::size_t sample_every = 0; // 这里的 sample 仅用于 ReplayEngine 回调测量，回测本身不依赖
  long double init_cash = 1'000'000.0L;

  std::size_t window = 200;
  double threshold = 0.001; // 0.1%
  std::int64_t trade_qty = 1;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--file" && i + 1 < argc) file = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::stod(argv[++i]);
    else if (a == "--sample" && i + 1 < argc) sample_every = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--cash" && i + 1 < argc) init_cash = static_cast<long double>(std::stold(argv[++i]));
    else if (a == "--window" && i + 1 < argc) window = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--th" && i + 1 < argc) threshold = std::stod(argv[++i]);
    else if (a == "--qty" && i + 1 < argc) trade_qty = static_cast<std::int64_t>(std::stoll(argv[++i]));
    else if (a == "--help") { usage(); return 0; }
    else { q::log::warn("Unknown arg: " + a); usage(); return 1; }
  }

  // --------- Components ---------
  q::market::ReplayConfig cfg{
    .path = file,
    .speed = speed,
    .print_every = false,
    .print_interval = 100000
  };
  q::market::ReplayEngine replay(cfg);

  q::book::FlatL2Book book;

  q::book::BookBuilder<q::book::FlatL2Book> book_builder(&book);
  MeanReversionStrategy strat(window, threshold, trade_qty);
  Oms oms;
  ExecutionSim exec;

  Portfolio pf;
  pf.cash = init_cash;

  Metrics mx;

  std::size_t trades = 0;
  std::size_t fills_count = 0;
  std::size_t market_views = 0;

  // --------- Run ---------
  q::LatencyRecorder cb_lat;
  const auto n = replay.run([&](const q::market::MarketEvent& e) {
    book_builder.on_event(e);

    if (q::book::BuildState::Live != book_builder.state() || book_builder.state() == q::book::BuildState::OutOfSync) return;

    auto top = book.top();
    if (!top.valid) return;

    MarketView mv;
    mv.ts_ns = e.ts_ns;
    mv.best_bid_px = top.bid_px;
    mv.best_ask_px = top.ask_px;
    mv.mid_px = (top.bid_px + top.ask_px) / 2;

    ++market_views;

    auto fill_events = exec.on_market(oms, mv);
    for (auto fill_event : fill_events) {
        pf.on_fill(fill_event);
        ++fills_count;

        // 把 leaves_after 反馈给策略（用于 Filled 时清 working id）
        if (auto* o = oms.get(fill_event.order_id)) {
            strat.on_fill(fill_event, o->leaves_qty);
        }
    }

    // 策略决定是否下单
    StratDecision dec = strat.on_market(mv, pf.position);
    // 3) 先撤单
    if (dec.cancel_buy) {
        auto id = strat.working_buy_id();
        if (id != 0) {
            auto up = exec.cancel(oms, mv, id);
            if (up.status == bt::OrderStatus::Canceled) strat.on_canceled(id);
        }
    }

    if (dec.cancel_sell) {
        auto id = strat.working_sell_id();
        if (id != 0) {
            auto up = exec.cancel(oms, mv, id);
            if (up.status == bt::OrderStatus::Canceled) strat.on_canceled(id);
        }
    }

    // 4) 再下单
    if (dec.submit) {
        auto res = exec.submit(oms, mv, *dec.submit);

        // accepted
        if (res.ack.status == bt::OrderStatus::Working || res.ack.status == bt::OrderStatus::PartiallyFilled) {
            strat.on_submit_accepted(dec.submit->side, res.order_id);
        }

        // submit 可能立刻产生 fill（partial/full）
        if (res.fill) {
            pf.on_fill(*res.fill);
            ++fills_count;

            if (auto* o = oms.get(res.fill->order_id)) {
                strat.on_fill(*res.fill, o->leaves_qty);
            }
        }
    }

    // 5) 更新净值
    mx.add_equity(pf.equity(mv.mid_px));
  }, (sample_every > 0 ? &cb_lat : nullptr), sample_every);

  // --------- Report ---------
  std::cout << "\n=== BACKTEST SUMMARY ===\n";
  std::cout << "events_processed=" << n << "\n";
  std::cout << "market_views=" << market_views << "\n";
  std::cout << "fills_count=" << fills_count << "\n";
  std::cout << "final_cash=" << static_cast<double>(pf.cash) << "\n";
  std::cout << "final_pos=" << pf.position << "\n";

  if (!mx.equity.empty()) {
    std::cout << "start_equity=" << static_cast<double>(mx.equity.front()) << "\n";
    std::cout << "end_equity=" << static_cast<double>(mx.equity.back()) << "\n";
  }
  std::cout << "total_return=" << static_cast<double>(mx.total_return()) << "\n";
  std::cout << "max_drawdown=" << static_cast<double>(mx.max_drawdown()) << "\n";
  std::cout << "sharpe(step)=" << mx.sharpe() << "\n";

  q::book::BuildState state = book_builder.state();
  std::cout << "\n=== BUILD STATS ===\n";
  std::cout << "live=" << (state == q::book::BuildState::Live)
            << " out_of_sync=" << (state == q::book::BuildState::OutOfSync)
            << " last_seq=" << book_builder.stats().last_seq
            << " gap=" << book_builder.stats().gap_count
            << " dup_old=" << book_builder.stats().dup_or_old_count
            << " crossed=" << book_builder.stats().crossed_count
            << " anomaly=" << book_builder.stats().anomaly_count
            << "\n";

  if (sample_every > 0) {
    auto st = cb_lat.compute();
    std::cout << "\n=== Replay callback latency (sampled) ===\n";
    std::cout << "samples=" << st.count
              << " p50(ns)=" << st.p50
              << " p99(ns)=" << st.p99
              << " p999(ns)=" << st.p999
              << " max(ns)=" << st.max
              << "\n";
  }

  return 0;
}
