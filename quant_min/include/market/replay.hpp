#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "common/latency.hpp"
#include "market/event.hpp"

namespace q::market {

struct ReplayConfig {
  std::string path;          // CSV 文件路径
  double speed = 0.0;        // 0 = fastest (no pacing)
  bool print_every = false;
  std::int64_t print_interval = 100000;
};

// CSV header:
// ts_ns,seq,kind,side,price,qty,action
// kind: SB,SL,SE,I
// side: B,A (optional for SB/SE)
// action: N,C,D (only for I)
class ReplayEngine {
public:
  explicit ReplayEngine(ReplayConfig cfg);

  // sample_every: 0 disables latency measurement
  std::size_t run(const std::function<void(const MarketEvent&)>& on_event,
                  q::LatencyRecorder* latency,
                  std::size_t sample_every);

private:
  ReplayConfig cfg_;
};

} // namespace q::market
