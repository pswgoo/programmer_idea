#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace q {

class LatencyRecorder {
public:
  void add_ns(std::int64_t ns) { samples_.push_back(ns); }

  struct Stats {
    std::int64_t p50{0}, p99{0}, p999{0}, max{0};
    std::size_t count{0};
  };

  Stats compute() const {
    Stats s{};
    s.count = samples_.size();
    if (samples_.empty()) return s;

    std::vector<std::int64_t> v = samples_;
    std::sort(v.begin(), v.end());

    auto pct = [&](double p) -> std::int64_t {
      const auto idx = static_cast<std::size_t>(p * (v.size() - 1));
      return v[idx];
    };

    s.p50  = pct(0.50);
    s.p99  = pct(0.99);
    s.p999 = pct(0.999);
    s.max  = v.back();
    return s;
  }

private:
  std::vector<std::int64_t> samples_;
};

} // namespace q
