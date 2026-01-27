#pragma once
#include "backtest3/types.hpp"

namespace bt3 {

struct SymbolContext {
  SymbolId sym{};
  MarketView last_mv{};

  void on_event(const TickEvent& ev) {
    last_mv.ts_ns = ev.ts_ns;
    last_mv.best_bid_px = ev.best_bid_px;
    last_mv.best_ask_px = ev.best_ask_px;
    last_mv.mid_px = ev.mid_px;
  }
};

} // namespace bt3
