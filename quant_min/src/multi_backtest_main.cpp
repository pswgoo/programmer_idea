#include <cstdint>
#include <iostream>
#include <vector>

#include "backtest3/engine.hpp"
#include "backtest3/replay.hpp"
#include "backtest3/types.hpp"

static std::vector<bt3::TickEvent> make_demo_stream(bt3::SymbolId sym,
                                                    std::int64_t start_ts,
                                                    int n,
                                                    std::int64_t base_px,
                                                    std::int64_t step_ns) {
  std::vector<bt3::TickEvent> v;
  v.reserve(n);
  std::int64_t ts = start_ts;
  std::int64_t px = base_px;
  for (int i = 0; i < n; ++i) {
    ts += step_ns;                  // e.g. 1ms
    px += (i % 2 == 0) ? 1 : -1;    // small oscillation
    bt3::TickEvent e;
    e.ts_ns = ts;
    e.sym = sym;
    e.mid_px = px;
    e.best_bid_px = px - 1;
    e.best_ask_px = px + 1;
    v.push_back(e);
  }
  return v;
}

int main() {
  using namespace bt3;

  std::vector<Instrument> instruments = {
      {1, "SYM1"},
      {2, "SYM2"},
      {3, "SYM3"},
      {4, "SYM4"},
  };

  // 让多个流在同一 ts 上有“碰撞”，这样 batch 才会 >1，能看到并行效果
  auto s1 = make_demo_stream(1, 0,        200000, 1000, 1'000'000); // 1ms
  auto s2 = make_demo_stream(2, 0,        200000, 2000, 1'000'000); // 同步
  auto s3 = make_demo_stream(3, 500'000,  200000, 3000, 1'000'000); // offset 0.5ms
  auto s4 = make_demo_stream(4, 0,        200000, 4000, 2'000'000); // 2ms

  VectorReplay r1(1, std::move(s1));
  VectorReplay r2(2, std::move(s2));
  VectorReplay r3(3, std::move(s3));
  VectorReplay r4(4, std::move(s4));

  std::vector<VectorReplay*> replays = {&r1, &r2, &r3, &r4};

  // 线程数你可以改成 1/2/4/8 对比（结果应一致）
  std::size_t n_threads = 4;

  MultiBacktestEngine engine(instruments, replays, n_threads);

  // demo initial positions
  engine.portfolio().set_position_idx(0, 10);
  engine.portfolio().set_position_idx(1, -5);
  engine.portfolio().set_position_idx(2, 3);
  engine.portfolio().set_position_idx(3, -2);

  engine.run();
  return 0;
}
