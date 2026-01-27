#include <cstdint>
#include <vector>

#include "backtest3/engine.hpp"
#include "backtest3/replay.hpp"
#include "backtest3/types.hpp"

static std::vector<bt3::TickEvent> make_stream(bt3::SymbolId sym,
                                               std::int64_t start_ts,
                                               int n,
                                               std::int64_t base_px,
                                               std::int64_t step_ns,
                                               std::int64_t wobble) {
  std::vector<bt3::TickEvent> v;
  v.reserve(n);
  std::int64_t ts = start_ts;
  std::int64_t px = base_px;
  for (int i = 0; i < n; ++i) {
    ts += step_ns;
    // 造点相关但有偏移的价差，让 pairs 有信号
    px += (i % 2 == 0) ? wobble : -wobble;

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
      {1, "A"},
      {2, "B"},
      {3, "C"},
      {4, "D"},
  };

  // A/B 做 pairs：让 A wobble=2，B wobble=1 形成价差波动
  auto a = make_stream(1, 0,       300000, 10000, 1'000'000, 2);
  auto b = make_stream(2, 0,       300000, 10002, 1'000'000, 1);
  auto c = make_stream(3, 500'000, 300000, 20000, 1'000'000, 1);
  auto d = make_stream(4, 0,       150000, 30000, 2'000'000, 1);

  VectorReplay r1(1, std::move(a));
  VectorReplay r2(2, std::move(b));
  VectorReplay r3(3, std::move(c));
  VectorReplay r4(4, std::move(d));

  std::vector<VectorReplay*> replays = {&r1, &r2, &r3, &r4};

  PairsConfig sc;
  sc.idx_a = 0;        // instruments[0] => SymbolId 1
  sc.idx_b = 1;        // instruments[1] => SymbolId 2
  sc.window = 200;
  sc.entry_z = 2.0;
  sc.exit_z = 0.5;
  sc.qty = 10;

  PortfolioRiskConfig rc;
  rc.max_gross_notional = 1'000'000'000; // 放大点，避免太早熔断
  rc.max_abs_position_per_sym = 1000;
  rc.max_order_qty = 200;
  rc.enable_drawdown_kill = false;

  ExecSimConfig ec;
  ec.fee_per_unit = 0;
  ec.slippage_ticks = 1;

  std::size_t n_threads = 4;

  MultiBacktestEngine engine(instruments, replays, n_threads, sc, rc, ec);

  // 初始仓位（默认 0 也可以）
  engine.set_position(1, 0);
  engine.set_position(2, 0);

  engine.run();
  return 0;
}
