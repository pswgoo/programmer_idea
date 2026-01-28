#include <cstdint>
#include <vector>
#include <iostream>

#include "backtest3/engine.hpp"                 // MultiSymbolEngine
#include "backtest3/strategy_portfolio.hpp"     // MeanReversionPortfolioStrategy
#include "backtest3/replay.hpp"                 // 你 backtest3 的 VectorReplay / FileReplay
#include "backtest3/scheduler.hpp"              // 必须支持 batch item = (sym_idx, MarketEvent)

// 你的项目一 MarketEvent
#include "market/event.hpp"

static std::vector<q::market::MarketEvent>
gen_stream(std::int64_t base_px,
           int n_ticks,
           std::int64_t start_ts,
           std::int64_t step_ns) {
  using namespace q::market;

  std::vector<MarketEvent> v;
  v.reserve(static_cast<std::size_t>(n_ticks) * 3 + 8);

  std::int64_t ts = start_ts;
  std::int64_t seq = 0;

  // Snapshot begin
  v.push_back(MarketEvent{ts, seq++, Kind::SnapshotBegin, Side::Unknown, 0, 0, Action::None});
  // Snapshot levels (bid/ask L1)
  v.push_back(MarketEvent{ts, seq++, Kind::SnapshotLevel, Side::Bid, base_px - 1, 100, Action::New});
  v.push_back(MarketEvent{ts, seq++, Kind::SnapshotLevel, Side::Ask, base_px + 1, 100, Action::New});
  // Snapshot end
  v.push_back(MarketEvent{ts, seq++, Kind::SnapshotEnd, Side::Unknown, 0, 0, Action::None});

  std::int64_t px = base_px;
  for (int i = 0; i < n_ticks; ++i) {
    ts += step_ns;
    // 让价格上下波动，产生均值回归信号
    px += (i % 2 == 0) ? 2 : -2;

    // bid/ask change
    v.push_back(MarketEvent{ts, seq++, Kind::Incremental, Side::Bid, px - 1, 100, Action::Change});
    v.push_back(MarketEvent{ts, seq++, Kind::Incremental, Side::Ask, px + 1, 100, Action::Change});
  }
  return v;
}

int main() {
  using namespace bt3;

  constexpr std::size_t N = 4;

  // 1) replays：每个 sym 一个 replay（MarketEvent 不带 symbol，靠 sym_idx 绑定）
  auto e0 = gen_stream(10000, 200000, 0,        1'000'000);
  auto e1 = gen_stream(20000, 200000, 0,        1'000'000);
  auto e2 = gen_stream(30000, 200000, 200'000,  1'000'000);
  auto e3 = gen_stream(40000, 100000, 0,        2'000'000);

  VectorReplay r0(std::move(e0));
  VectorReplay r1(std::move(e1));
  VectorReplay r2(std::move(e2));
  VectorReplay r3(std::move(e3));

  std::vector<VectorReplay*> replays = {&r0, &r1, &r2, &r3};

  // 2) scheduler：必须输出 batch item = (sym_idx, MarketEvent)
  SymBatchScheduler sched(replays, N);  // 你 backtest3/scheduler.hpp 里实现这个类

  // 3) engine config：worker 绑定
  EngineConfig ec;
  ec.n_workers = 4;

  // 4) project2 exec/risk config（你自己调）
  bt::ExecConfig exec_cfg;
  exec_cfg.allow_taker_fill = true;
  exec_cfg.enable_partial_fill = true;
  exec_cfg.max_fill_qty_per_tick = 2;     // 强制 partial fill
  exec_cfg.cancel_delay_base_ns = 1'000'000;
  exec_cfg.cancel_delay_jitter_ns = 4'000'000;

  bt::RiskConfig risk_cfg;
  // 按你项目二 risk 默认即可，这里略

  // 5) engine + strategy
  MultiSymbolEngine engine(N, ec, exec_cfg, risk_cfg);

  MeanRevPortfolioConfig sc;
  sc.window = 200;
  sc.threshold = 0.001;
  sc.trade_qty = 10;
  sc.reprice_after_ns = 5'000'000;

  MeanReversionPortfolioStrategy strat(N, sc);

  engine.run(sched, strat);

  return 0;
}
