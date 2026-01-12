#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "book/flat_l2_book.hpp"
#include "book/l2_book.hpp"     // map版：q::book::L2Book
#include "common/clock.hpp"
#include "common/latency.hpp"
#include "common/log.hpp"
#include "common/spsc_ring.hpp"
#include "common/backoff.hpp"
#include "market/replay.hpp"
#include "book/book_builder.hpp"

static void usage() {
  std::cout
    << "Usage:\n"
    << "  ./quant_min --file <csv> [--speed 0|0.1|1] [--book map|flat]\n"
    << "            [--pipeline direct|spsc] [--ring <pow2>]\n"
    << "            [--sample K] [--print-every N]\n\n"
    << "Notes:\n"
    << "  --pipeline direct : single-thread replay->book (baseline)\n"
    << "  --pipeline spsc   : two-thread replay (producer) -> ring -> book (consumer)\n"
    << "  --ring            : ring capacity, must be power-of-two (default 1048576)\n"
    << "  --sample K        : sample book update latency every K ticks in consumer (default 100)\n"
    << "                     0 disables latency measurement.\n"
    << "  For container comparison, prefer: --speed 0\n";
}

struct PipeStats {
  std::size_t ticks{0};
  double seconds{0.0};
  double rate{0.0};

  std::size_t ring_max_depth{0};
  std::size_t ring_full_count{0};

  q::book::BuildState build_state;
  q::book::BuilderStats build_stats;

  q::LatencyRecorder::Stats book_lat{};
};

template <class BookT>
static PipeStats run_direct(const q::market::ReplayConfig& cfg, std::size_t sample_every) {
  BookT book;
  q::book::BookBuilder<BookT> book_builder(&book);
  q::LatencyRecorder lat;

  q::market::ReplayEngine engine(cfg);

  std::size_t n = 0;
  const auto wall0 = q::now();
  n = engine.run([&](const q::market::MarketEvent& t) {
      book_builder.on_event(t);
    },
    (sample_every > 0 ? &lat : nullptr),
    sample_every
  );
  const auto wall1 = q::now();

  const auto wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall1 - wall0).count();
  const double secs = static_cast<double>(wall_ns) / 1e9;
  const double rate = secs > 0.0 ? static_cast<double>(n) / secs : 0.0;

  PipeStats ps{};
  ps.ticks = n;
  ps.seconds = secs;
  ps.rate = rate;
  ps.book_lat = lat.compute();
  ps.build_state = book_builder.state();
  ps.build_stats = book_builder.stats();
  return ps;
}

template <class BookT>
static PipeStats run_spsc(const q::market::ReplayConfig& cfg,
                          std::size_t ring_cap_pow2,
                          std::size_t sample_every) {
  using Event = q::market::MarketEvent;

  q::SpscRing<Event> ring(ring_cap_pow2);

  std::atomic<bool> producer_done{false};

  std::atomic<std::size_t> produced{0};
  std::atomic<std::size_t> consumed{0};
  std::atomic<std::size_t> ring_full_count{0};
  std::atomic<std::size_t> ring_max_depth{0};

  q::LatencyRecorder book_lat;

  // Consumer thread: pop -> book update
  BookT book;
  q::book::BookBuilder book_builder(&book);
  std::thread consumer([&] {
    Event t{};
    std::size_t local_consumed = 0;
    q::IdleBackoff idleBackoff(2500, 2500, 100, 2000);

    while (true) {
      if (ring.pop(t)) {
        idleBackoff.reset();
        // measure book update latency (sampling)
        if (sample_every > 0 && ((local_consumed % sample_every) == 0)) {
          const auto t0 = q::now();
          book_builder.on_event(t);
          const auto t1 = q::now();
          book_lat.add_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        } else {
          book_builder.on_event(t);
        }

        ++local_consumed;
        consumed.store(local_consumed, std::memory_order_relaxed);
        continue;
      }

      // ring empty
      if (producer_done.load(std::memory_order_acquire) && ring.empty()) {
        break;
      }

      // backoff: yield to reduce busy wait impact
      idleBackoff.idle();
    }
  });

  // Producer thread: replay -> push
  std::thread producer([&] {
    q::market::ReplayEngine engine(cfg);

    std::size_t local_produced = 0;

    // Use ReplayEngine as reader + pacing; callback only pushes to ring.
    // Disable latency measurement in producer.
    (void)engine.run([&](const Event& t) {
      // backpressure: wait until there is space
      while (!ring.push(t)) {
        ring_full_count.fetch_add(1, std::memory_order_relaxed);
        // update max depth too (optional)
        const auto d = ring.size_approx();
        auto cur = ring_max_depth.load(std::memory_order_relaxed);
        while (d > cur && !ring_max_depth.compare_exchange_weak(cur, d, std::memory_order_relaxed)) {}
        std::this_thread::yield();
      }

      ++local_produced;
      produced.store(local_produced, std::memory_order_relaxed);

      // update max depth
      const auto d = ring.size_approx();
      auto cur = ring_max_depth.load(std::memory_order_relaxed);
      while (d > cur && !ring_max_depth.compare_exchange_weak(cur, d, std::memory_order_relaxed)) {}
    }, nullptr, 0);

    producer_done.store(true, std::memory_order_release);
  });

  const auto wall0 = q::now();
  producer.join();
  consumer.join();
  const auto wall1 = q::now();

  const auto n = consumed.load(std::memory_order_relaxed);
  const auto wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall1 - wall0).count();
  const double secs = static_cast<double>(wall_ns) / 1e9;
  const double rate = secs > 0.0 ? static_cast<double>(n) / secs : 0.0;

  PipeStats ps{};
  ps.ticks = n;
  ps.seconds = secs;
  ps.rate = rate;
  ps.ring_full_count = ring_full_count.load(std::memory_order_relaxed);
  ps.ring_max_depth  = ring_max_depth.load(std::memory_order_relaxed);
  ps.book_lat = book_lat.compute();
  ps.build_state = book_builder.state();
  ps.build_stats = book_builder.stats();
  return ps;
}

static void print_stats(const std::string& label, const PipeStats& s, bool latency_enabled) {
  std::cout << "\n=== " << label << " ===\n";
  std::cout << "ticks=" << s.ticks << " time=" << s.seconds << "s rate=" << s.rate << " msg/s\n";
  if (label.find("spsc") != std::string::npos) {
    std::cout << "ring_max_depth=" << s.ring_max_depth
            << " ring_full_count=" << s.ring_full_count << "\n";

  }
  std::cout << "build: live=" << (s.build_state == q::book::BuildState::Live)
            << " out_of_sync=" << (s.build_state == q::book::BuildState::OutOfSync)
            << " last_seq=" << s.build_stats.last_seq
            << " gap=" << s.build_stats.gap_count
            << " dup_old=" << s.build_stats.dup_or_old_count
            << " crossed=" << s.build_stats.crossed_count
            << " anomaly=" << s.build_stats.anomaly_count << "\n";
  if (latency_enabled) {
    std::cout << "book_update_latency(ns): samples=" << s.book_lat.count
              << " p50=" << s.book_lat.p50
              << " p99=" << s.book_lat.p99
              << " p999=" << s.book_lat.p999
              << " max=" << s.book_lat.max
              << "\n";
  } else {
    std::cout << "book_update_latency: disabled (--sample 0)\n";
  }
}

int main(int argc, char** argv) {
  std::string file = "data/sample_ticks.csv";
  double speed = 0.0;
  std::string book_type = "flat";
  std::string pipeline = "direct";
  std::size_t ring_cap = 1u << 20; // 1048576
  std::size_t sample_every = 100;
  bool print_every = false;
  std::int64_t print_interval = 100000;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--file" && i + 1 < argc) file = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::stod(argv[++i]);
    else if (a == "--book" && i + 1 < argc) book_type = argv[++i];
    else if (a == "--pipeline" && i + 1 < argc) pipeline = argv[++i];
    else if (a == "--ring" && i + 1 < argc) ring_cap = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--sample" && i + 1 < argc) sample_every = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (a == "--print-every" && i + 1 < argc) { print_every = true; print_interval = std::stoll(argv[++i]); }
    else if (a == "--help") { usage(); return 0; }
    else {
      q::log::warn("Unknown argument: " + a);
      usage();
      return 1;
    }
  }

  if (!q::is_power_of_two(ring_cap)) {
    q::log::warn("--ring must be a power-of-two, e.g. 65536, 1048576, 4194304");
    return 1;
  }

  q::market::ReplayConfig cfg{
    .path = file,
    .speed = speed,
    .print_every = print_every,
    .print_interval = print_interval
  };

  const bool latency_enabled = (sample_every > 0);

  // Dispatch by book type and pipeline
  if (book_type == "map") {
    if (pipeline == "direct") {
      auto s = run_direct<q::book::L2Book>(cfg, sample_every);
      print_stats("direct + map", s, latency_enabled);
      return 0;
    } else if (pipeline == "spsc") {
      auto s = run_spsc<q::book::L2Book>(cfg, ring_cap, sample_every);
      print_stats("spsc + map", s, latency_enabled);
      return 0;
    }
  } else if (book_type == "flat") {
    if (pipeline == "direct") {
      auto s = run_direct<q::book::FlatL2Book>(cfg, sample_every);
      print_stats("direct + flat", s, latency_enabled);
      return 0;
    } else if (pipeline == "spsc") {
      auto s = run_spsc<q::book::FlatL2Book>(cfg, ring_cap, sample_every);
      print_stats("spsc + flat", s, latency_enabled);
      return 0;
    }
  }

  q::log::warn("Invalid combination. Use --book map|flat and --pipeline direct|spsc");
  usage();
  return 1;
}
