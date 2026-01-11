#pragma once
#include <chrono>
#include <thread>
#include <cstdint>

namespace q {

// 在 SPSC ring consumer 实现自适应 idle backoff（spin/yield/sleep），在低负载时显著降低 CPU 占用，同时保持行情突发时低延迟恢复。
class IdleBackoff {
public:
  // spin_iters: 空队列时先空转多少次
  // yield_iters: 然后 yield 多少次
  // sleep_us: 再进入固定微秒级 sleep（逐步增长也可）
  explicit IdleBackoff(std::uint32_t spin_iters = 2000,
                       std::uint32_t yield_iters = 2000,
                       std::uint32_t sleep_us = 50,
                       std::uint32_t max_sleep_us = 500)
      : spin_iters_(spin_iters),
        yield_iters_(yield_iters),
        sleep_us_(sleep_us),
        max_sleep_us_(max_sleep_us) {}

  // 每次 ring 空时调用一次
  void idle() {
    if (spins_ < spin_iters_) {
      ++spins_;
      // busy spin: do nothing
      return;
    }

    if (yields_ < yield_iters_) {
      ++yields_;
      std::this_thread::yield();
      return;
    }

    // sleep阶段：指数退避到 max_sleep_us
    std::this_thread::sleep_for(std::chrono::microseconds(cur_sleep_us_));
    if (cur_sleep_us_ < max_sleep_us_) {
      cur_sleep_us_ = std::min<std::uint32_t>(max_sleep_us_, cur_sleep_us_ * 2);
    }
  }

  // 一旦成功 pop 到消息，就 reset（恢复低延迟）
  void reset() {
    spins_ = 0;
    yields_ = 0;
    cur_sleep_us_ = sleep_us_;
  }

private:
  std::uint32_t spin_iters_{2000};
  std::uint32_t yield_iters_{2000};
  std::uint32_t sleep_us_{50};
  std::uint32_t max_sleep_us_{500};

  std::uint32_t spins_{0};
  std::uint32_t yields_{0};
  std::uint32_t cur_sleep_us_{50};
};

} // namespace q
