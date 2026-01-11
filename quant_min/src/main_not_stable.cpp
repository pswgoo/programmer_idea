#include <iostream>
#include <string>

#include "book/l2_book.hpp"          // map版：q::book::L2Book
#include "book/flat_l2_book.hpp"     // flat版：q::book::FlatL2Book
#include "common/latency.hpp"
#include "common/log.hpp"
#include "market/replay.hpp"

static void usage() {
  std::cout
    << "Usage:\n"
    << "  ./quant_min --file data/sample_ticks.csv [--speed 0|0.1|1|10] [--book map|flat] [--print-every N]\n\n"
    << "Notes:\n"
    << "  --speed 0     : fastest (no pacing)\n"
    << "  --speed 1     : realtime pacing\n"
    << "  --speed 0.1   : 10x faster than realtime\n"
    << "  --book map    : std::map L2 book baseline\n"
    << "  --book flat   : cache-friendly flat vector L2 book\n";
}

template <class BookT>
int run_with_book(const q::market::ReplayConfig& cfg) {
  BookT book;
  q::LatencyRecorder latency;

  q::market::ReplayEngine engine(cfg);
  const auto n = engine.run([&](const q::market::Tick& t) {
    book.on_tick(t);
  }, latency);

  const auto st = latency.compute();
  q::log::info("Callback latency(ns): count=" + std::to_string(st.count) +
               " p50=" + std::to_string(st.p50) +
               " p99=" + std::to_string(st.p99) +
               " p999=" + std::to_string(st.p999) +
               " max=" + std::to_string(st.max));

  auto top = book.top();
  if (top.valid) {
    std::cout << "Top of book:\n"
              << "  bid " << top.bid_px << " x " << top.bid_qty << "\n"
              << "  ask " << top.ask_px << " x " << top.ask_qty << "\n";
  } else {
    std::cout << "Book empty / not enough data.\n";
  }

  std::cout << "ticks processed: " << n << "\n";
  return 0;
}

int main(int argc, char** argv) {
  std::string file = "data/sample_ticks.csv";
  double speed = 0.0;
  std::string book_type = "flat";
  bool print_every = false;
  std::int64_t print_interval = 100000;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--file" && i + 1 < argc) file = argv[++i];
    else if (a == "--speed" && i + 1 < argc) speed = std::stod(argv[++i]);
    else if (a == "--book" && i + 1 < argc) book_type = argv[++i];
    else if (a == "--print-every" && i + 1 < argc) { print_every = true; print_interval = std::stoll(argv[++i]); }
    else if (a == "--help") { usage(); return 0; }
  }

  q::market::ReplayConfig cfg{
    .path = file,
    .speed = speed,
    .print_every = print_every,
    .print_interval = print_interval
  };

  if (book_type == "map") {
    return run_with_book<q::book::L2Book>(cfg);
  }
  if (book_type == "flat") {
    return run_with_book<q::book::FlatL2Book>(cfg);
  }

  q::log::warn("Unknown --book type. Use map|flat.");
  usage();
  return 1;
}
