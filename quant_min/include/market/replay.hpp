#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "common/latency.hpp"
#include "market/event.hpp"

namespace q::market {

struct ReplayConfig {
  std::string path;          // CSV 文件路径
  double speed = 0.0;        // 0 = 不sleep（最快回放），1=原速，10=10倍速（更慢），0.1=10倍加速
  bool print_every = false;
  std::int64_t print_interval = 100000; // 每N条打印一次（避免刷屏）
};

// CSV 格式（含表头）
// ts_ns,side,price,qty
// 1700000000000000000,B,100000,5
class ReplayEngine {
public:
  explicit ReplayEngine(ReplayConfig cfg);

  // on_tick：用户处理 tick 的回调
  // 返回：处理的 tick 数量
  std::size_t run(const std::function<void(const Tick&)>& on_tick, q::LatencyRecorder& latency, std::size_t sample_every);

private:
  ReplayConfig cfg_;
};

} // namespace q::market
