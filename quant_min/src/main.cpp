#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "book/flat_l2_book.hpp"
#include "book/l2_book.hpp"      // map版：q::book::L2Book
#include "common/clock.hpp"
#include "common/log.hpp"
#include "market/replay.hpp"

struct RunResult {
  std::size_t ticks{};
  double seconds{};
  double rate{}; // msg/s
  q::LatencyRecorder::Stats lat{};
};

static void usage() {
  std::cout
    << "Usage:\n"
    << "  ./quant_min --file <csv> [--speed 0|0.1|1|10] [--book map|flat]\n"
    << "            [--warmup N] [--repeat N] [--sample K] [--print-every N]\n\n"
    << "Notes:\n"
    << "  --speed 0     : fastest (no pacing)\n"
    << "  --speed 1     : realtime pacing\n"
    << "  --speed 0.1   : 10x faster than realtime\n"
    << "  --book map    : std::map baseline\n"
    << "  --book flat   : flat vector cache-friendly\n"
    << "  --warmup N    : warmup runs not included in summary (default 1)\n"
    << "  --repeat N    : measured runs in summary (default 7)\n"
    << "  --sample K    : sample latency every K ticks (default 100). 0 disables latency.\n"
    << "                 (Sampling reduces now() overhead and stabilizes p99/p999.)\n";
}

static double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const std::size_t m = v.size() / 2;
  if (v.size() % 2 == 1) return v[m];
  return 0.5 * (v[m - 1] + v[m]);
}

static double minv(const std::vector<double>& v) {
  return v.empty() ? 0.0 : *std::min_element(v.begin(), v.end());
}

static double maxv(const std::vector<double>& v) {
  return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end());
}

template <class BookT>
static RunResult run_once(const q::market::ReplayConfig& cfg, std::size_t sample_every) {
  BookT book;
  q::LatencyRecorder latency;

  q::market::ReplayEngine engine(cfg);

  const auto wall0 = q::now();
  const auto n = engine.run(
      [&](const q::market::Tick& t) { book.on_tick(t); },
      latency,
      sample_every);
  const auto wall1 = q::now();

  const auto wall_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(wall1 - wall0).count();
  const double secs = static_cast<double>(wall_ns) / 1e9;
  const double rate = secs > 0.0 ? static_cast<double>(n) / secs : 0.0;

  RunResult rr{};
  rr.ticks = n;
  rr.seconds = secs;
  rr.rate = rate;
  rr.lat = latency.compute(); // if sample_every==0, count will be 0
  return rr;
}

template <class BookT>
static int benchmark(const q::market::ReplayConfig& cfg,
                     int warmup,
                     int repeat,
                     std::size_t sample_every,
                     const std::string& label) {
  // Warmup
  for (int i = 0; i < warmup; ++i) {
    (void)run_once<BookT>(cfg, sample_every);
  }

  std::vector<double> rates;
  std::vector<double> p50s, p99s, p999s, maxs;

  for (int i = 0; i < repeat; ++i) {
    auto r = run_once<BookT>(cfg, sample_every);

    rates.push_back(r.rate);

    if (sample_every > 0) {
      p50s.push_back(static_cast<double>(r.lat.p50));
      p99s.push_back(static_cast<double>(r.lat.p99));
      p999s.push_back(static_cast<double>(r.lat.p999));
      maxs.push_back(static_cast<double>(r.lat.max));
    }

    std::cout << "[" << label << " run " << (i + 1) << "/" << repeat << "] "
              << "ticks=" << r.ticks << " "
              << "time=" << r.seconds << "s "
              << "rate=" << r.rate << " msg/s";

    if (sample_every > 0) {
      std::cout << "  lat(ns):"
                << " p50=" << r.lat.p50
                << " p99=" << r.lat.p99
                << " p999=" << r.lat.p999
                << " max=" << r.lat.max
                << " (samples=" << r.lat.count << ")";
    } else {
      std::cout << "  lat(ns): disabled (--sample 0)";
    }

    std::cout << "\n";
  }

  std::cout << "\n=== SUMMARY (" << label << ") ===\n";
  std::cout << "rate msg/s: median=" << median(rates)
            << " min=" << minv(rates)
            << " max=" << maxv(rates) << "\n";

  if (sample_every > 0) {
    std::cout << "p50  ns  : median=" << median(p50s)
              << " min=" << minv(p50s)
              << " max=" << maxv(p50s) << "\n";
    std::cout << "p99  ns  : median=" << median(p99s)
              << " min=" << minv(p99s)
              << " max=" << maxv(p99s) << "\n";
    std::cout << "p999 ns  : median=" << median(p999s)
              << " min=" << minv(p999s)
              << " max=" << maxv(p999s) << "\n";
    std::cout << "max  ns  : median=" << median(maxs)
              << " min=" << minv(maxs)
              << " max=" << maxv(maxs) << "\n";
  }

  std::cout << "\n";
  return 0;
}

int main(int argc, char** argv) {
  std::string file = "data/sample_ticks.csv";
  double speed = 0.0;
  std::string book_type = "flat";
  int warmup = 1;
  int repeat = 7;
  std::size_t sample_every = 100; // 1% sampling by default
  bool print_every = false;
  std::int64_t print_interval = 100000;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--file" && i + 1 < argc) file = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::stod(argv[++i]);
    else if (a == "--book" && i + 1 < argc) book_type = argv[++i];
    else if (a == "--warmup" && i + 1 < argc) warmup = std::stoi(argv[++i]);
    else if (a == "--repeat" && i + 1 < argc) repeat = std::stoi(argv[++i]);
    else if (a == "--sample" && i + 1 < argc) sample_every = static_cast<std::size_t>(std::stoll(argv[++i]));
    else if (a == "--print-every" && i + 1 < argc) { print_every = true; print_interval = std::stoll(argv[++i]); }
    else if (a == "--help") { usage(); return 0; }
    else {
      q::log::warn("Unknown argument: " + a);
      usage();
      return 1;
    }
  }

  if (warmup < 0 || repeat <= 0) {
    q::log::warn("--warmup must be >= 0 and --repeat must be > 0");
    return 1;
  }

  q::market::ReplayConfig cfg{
    .path = file,
    .speed = speed,
    .print_every = print_every,
    .print_interval = print_interval
  };

  // 对容器/数据结构 benchmark：强烈建议 speed=0
  if (speed != 0.0) {
    q::log::warn("For data structure benchmark, consider using --speed 0 to reduce pacing noise.");
  }

  if (book_type == "map") {
    return benchmark<q::book::L2Book>(cfg, warmup, repeat, sample_every, "map");
  }
  if (book_type == "flat") {
    return benchmark<q::book::FlatL2Book>(cfg, warmup, repeat, sample_every, "flat");
  }

  q::log::warn("Unknown --book type. Use map|flat.");
  usage();
  return 1;
}
