#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

namespace q {

// helper
inline bool is_power_of_two(std::size_t x) {
  return x >= 2 && (x & (x - 1)) == 0;
}

// SPSC ring buffer (Single Producer Single Consumer)
// - Fixed capacity at runtime, must be power-of-two
// - Lock-free with acquire/release ordering
// - Stores T in a contiguous buffer (cache-friendly)
// Notes:
// - push/pop are wait-free under SPSC assumptions
// - size() is approximate but accurate enough for stats
template <class T>
class SpscRing {
public:
  explicit SpscRing(std::size_t capacity_pow2)
      : cap_(capacity_pow2), mask_(capacity_pow2 - 1), buf_(capacity_pow2) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "For simplicity, SpscRing<T> requires trivially copyable T (Tick fits).");
    if (!is_power_of_two(capacity_pow2)) {
      throw std::bad_alloc(); // keep it simple; you can throw std::invalid_argument if you prefer
    }
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  std::size_t capacity() const { return cap_; }

  // Producer thread only
  bool push(const T& v) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next = head + 1;

    // full if next == tail + cap
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (next - tail > cap_) return false;

    buf_[head & mask_] = v;
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer thread only
  bool pop(T& out) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (tail == head) return false;

    out = buf_[tail & mask_];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  // Approximate size (safe enough for telemetry)
  std::size_t size_approx() const {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    return head - tail;
  }

  bool empty() const { return size_approx() == 0; }

private:
  const std::size_t cap_;
  const std::size_t mask_;

  // Prevent false sharing between producer and consumer indices
  alignas(64) std::atomic<std::size_t> head_{0};
  char pad1_[64 - sizeof(std::atomic<std::size_t>)]{};
  alignas(64) std::atomic<std::size_t> tail_{0};
  char pad2_[64 - sizeof(std::atomic<std::size_t>)]{};

  std::vector<T> buf_;
};

} // namespace q
