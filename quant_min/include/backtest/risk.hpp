#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "backtest/orders.hpp"
#include "backtest/oms.hpp"
#include "backtest/market_view.hpp"

namespace bt {

struct RiskConfig {
  // --- exposure ---
  std::int64_t max_abs_position{100};      // 最大绝对持仓（股/张）
  std::int64_t max_order_qty{50};          // 单笔最大下单量
  std::int64_t max_active_orders{2};       // 同时活跃挂单数（全局）
  std::int64_t max_active_orders_per_side{1}; // 每个方向最多活跃挂单数

  // --- order rate limiting ---
  // 每秒最多发多少 submit（cancel 不计入，或者你也可以计入）
  std::int64_t max_submits_per_sec{50};

  // --- kill switch ---
  std::int64_t max_consecutive_rejects{20};     // 连续被风控/执行拒单次数上限
  bool enable_kill_switch{true};

  // --- optional: strategy sanity ---
  bool require_valid_market{true}; // 没有 bid/ask 时拒单
};

class RiskManager {
public:
  explicit RiskManager(RiskConfig cfg = {}) : cfg_(cfg) {}

  struct Decision {
    bool ok{true};
    std::string reason;
  };

  // 下单前检查：通过返回 {ok=true}；拒绝则 ok=false & reason
  Decision pre_submit_check(const Oms& oms,
                            const MarketView& mv,
                            const OrderRequest& req,
                            std::int64_t current_position) {
    if (killed_) return {false, "killed"};

    if (cfg_.require_valid_market) {
      if (mv.best_bid_px <= 0 || mv.best_ask_px <= 0) return reject("no_market");
    }

    if (req.qty <= 0) return reject("qty<=0");
    if (req.qty > cfg_.max_order_qty) return reject("qty>max_order_qty");
    if (req.limit_px <= 0) return reject("limit_px<=0");

    // 活跃挂单数限制
    const auto active = count_active_orders(oms);
    if (active >= cfg_.max_active_orders) return reject("too_many_active_orders");

    const auto active_side = count_active_orders_side(oms, req.side);
    if (active_side >= cfg_.max_active_orders_per_side) return reject("too_many_active_orders_side");

    // 下单频率限制（按 mv.ts_ns）
    if (!rate_limit_ok(mv.ts_ns)) return reject("rate_limited");

    // 仓位限制：用“最坏情况”估算（假设该单全部成交）
    std::int64_t next_pos = current_position;
    if (req.side == Side::Buy) next_pos += req.qty;
    else next_pos -= req.qty;

    if (std::llabs(next_pos) > cfg_.max_abs_position) return reject("position_limit");

    // 通过
    record_submit(mv.ts_ns);
    return {true, ""};
  }

  // 记录执行层的拒单（例如 ExecutionSim 返回 ack=Rejected）
  void on_exec_reject(std::int64_t ts_ns, const std::string& reason) {
    (void)ts_ns;
    ++consecutive_rejects_;
    last_reject_reason_ = reason;
    if (cfg_.enable_kill_switch && consecutive_rejects_ >= cfg_.max_consecutive_rejects) {
      killed_ = true;
    }
  }

  // 一旦出现一次正常成交/正常ack，也可以把连续拒单计数清掉（更合理）
  void on_good_event() {
    consecutive_rejects_ = 0;
    last_reject_reason_.clear();
  }

  bool killed() const { return killed_; }
  std::int64_t consecutive_rejects() const { return consecutive_rejects_; }
  const std::string& last_reject_reason() const { return last_reject_reason_; }

private:
  Decision reject(const char* r) {
    ++consecutive_rejects_;
    last_reject_reason_ = r;
    if (cfg_.enable_kill_switch && consecutive_rejects_ >= cfg_.max_consecutive_rejects) killed_ = true;
    return {false, r};
  }

  static bool is_active(const Order& o) {
    return o.status == OrderStatus::Working ||
           o.status == OrderStatus::PartiallyFilled ||
           o.status == OrderStatus::CancelRequested;
  }

  static std::int64_t count_active_orders(const Oms& oms) {
    std::int64_t c = 0;
    // 这里利用 active_orders()（它包含 Working/PartiallyFilled/CancelRequested）
    for (auto* o : const_cast<Oms&>(oms).active_orders()) { // active_orders 不是 const；先这么用最小改动
      if (o && is_active(*o)) ++c;
    }
    return c;
  }

  static std::int64_t count_active_orders_side(const Oms& oms, Side side) {
    std::int64_t c = 0;
    for (auto* o : const_cast<Oms&>(oms).active_orders()) {
      if (o && is_active(*o) && o->side == side) ++c;
    }
    return c;
  }

  bool rate_limit_ok(std::int64_t ts_ns) const {
    // 一秒窗口：统计最近 1s 内的 submit 次数
    const std::int64_t window = 1'000'000'000LL;
    // 允许 ts 不严格单调，但你这里是回放/事件驱动，基本单调
    // 清理旧记录
    auto it = std::lower_bound(submit_ts_.begin(), submit_ts_.end(), ts_ns - window);
    const auto recent = static_cast<std::int64_t>(submit_ts_.end() - it);
    return recent < cfg_.max_submits_per_sec;
  }

  void record_submit(std::int64_t ts_ns) {
    submit_ts_.push_back(ts_ns);
    // 控制数组大小：只保留最近 2s 的即可
    const std::int64_t keep = 2'000'000'000LL;
    auto it = std::lower_bound(submit_ts_.begin(), submit_ts_.end(), ts_ns - keep);
    if (it != submit_ts_.begin()) submit_ts_.erase(submit_ts_.begin(), it);
  }

private:
  RiskConfig cfg_;

  bool killed_{false};
  std::int64_t consecutive_rejects_{0};
  std::string last_reject_reason_;

  // submit 时间戳序列（用于 rate limiting）
  std::vector<std::int64_t> submit_ts_;
};

} // namespace bt
