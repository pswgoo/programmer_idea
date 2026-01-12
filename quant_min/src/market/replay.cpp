#include "market/replay.hpp"

#include <charconv>
#include <chrono>
#include <thread>

#include "common/clock.hpp"
#include "common/csv.hpp"
#include "common/log.hpp"

namespace {

inline bool parse_i64(std::string_view s, std::int64_t& out) {
  const char* b = s.data();
  const char* e = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(b, e, out);
  return ec == std::errc{} && ptr == e;
}

inline bool parse_kind(std::string_view s, q::market::Kind& out) {
  if (s == "SB") { out = q::market::Kind::SnapshotBegin; return true; }
  if (s == "SL") { out = q::market::Kind::SnapshotLevel; return true; }
  if (s == "SE") { out = q::market::Kind::SnapshotEnd; return true; }
  if (s == "I")  { out = q::market::Kind::Incremental; return true; }
  return false;
}

inline bool parse_side(std::string_view s, q::market::Side& out) {
  if (s.empty()) { out = q::market::Side::Unknown; return true; }
  if (s == "B" || s == "Bid" || s == "bid") { out = q::market::Side::Bid; return true; }
  if (s == "A" || s == "Ask" || s == "ask") { out = q::market::Side::Ask; return true; }
  return false;
}

inline bool parse_action(std::string_view s, q::market::Action& out) {
  if (s.empty()) { out = q::market::Action::None; return true; }
  if (s == "N") { out = q::market::Action::New; return true; }
  if (s == "C") { out = q::market::Action::Change; return true; }
  if (s == "D") { out = q::market::Action::Delete; return true; }
  return false;
}

} // namespace

namespace q::market {

ReplayEngine::ReplayEngine(ReplayConfig cfg) : cfg_(std::move(cfg)) {}

std::size_t ReplayEngine::run(const std::function<void(const MarketEvent&)>& on_event,
                              q::LatencyRecorder* latency,
                              std::size_t sample_every) {
  q::CsvReader reader(cfg_.path);
  if (!reader.good()) {
    q::log::warn("Failed to open replay file.");
    return 0;
  }

  std::size_t n = 0;
  std::int64_t first_ts = -1, prev_ts = -1;
  TimePoint wall_start;

  while (auto line = reader.next_line()) {
    auto fields = q::split_csv_line(*line);
    if (fields.size() < 7) continue;

    // header
    if (n == 0 && (fields[0] == "ts_ns" || fields[2] == "kind")) {
      continue;
    }

    MarketEvent e{};
    if (!parse_i64(fields[0], e.ts_ns)) continue;
    if (!parse_i64(fields[1], e.seq)) continue;
    if (!parse_kind(fields[2], e.kind)) continue;
    if (!parse_side(fields[3], e.side)) continue;

    // price/qty can be empty for SB/SE
    if (!fields[4].empty() && !parse_i64(fields[4], e.price)) continue;
    if (!fields[5].empty() && !parse_i64(fields[5], e.qty)) continue;

    if (!parse_action(fields[6], e.action)) continue;

    if (first_ts < 0) {
        first_ts = e.ts_ns;
        wall_start = q::now();
    }

    // optional pacing
    if (cfg_.speed > 0.0 && prev_ts >= 0) {
    //   const auto dt_ns = t.ts_ns - prev_ts;
    //   if (dt_ns > 0) {
    //     // 速度：speed=1 原速；speed=0.1 -> 加速10倍（sleep更少）
    //     const auto sleep_ns = static_cast<std::int64_t>(static_cast<double>(dt_ns) * cfg_.speed);
    //     if (sleep_ns > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
    //   }
        std::int64_t logic_dt = e.ts_ns - first_ts;
        auto target = wall_start + std::chrono::nanoseconds(static_cast<std::int64_t>(logic_dt * cfg_.speed));
        while (true) {
            auto now = q::now();
            if (target <= now) break;
            auto diff = target - now;
            if (diff >= std::chrono::microseconds(200)) {
                std::this_thread::sleep_for(diff - std::chrono::microseconds(100));
                break;
            }
            // continue loop
        }
    }
    prev_ts = e.ts_ns;

    // measure callback latency
    // 抽样测 callback 延迟
    if (latency && sample_every > 0 && ((n % sample_every) == 0)) {
      const auto t0 = q::now();
      on_event(e);
      const auto t1 = q::now();
      latency->add_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    } else {
      on_event(e);
    }

    ++n;

    if (cfg_.print_every && (n % static_cast<std::size_t>(cfg_.print_interval) == 0)) {
      q::log::info("Processed events: " + std::to_string(n));
    }
  }

  const auto wall_end = q::now();
  const auto wall_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();

  if (n > 0) {
    const double secs = static_cast<double>(wall_ns) / 1e9;
    const double rate = secs > 0 ? static_cast<double>(n) / secs : 0.0;
    q::log::info("Replay done. ticks=" + std::to_string(n) + " rate=" + std::to_string(rate) + " msg/s, cost " + std::to_string(secs) + " seconds");
  }

  return n;
}

} // namespace q::market
