#pragma once
#include <cstddef>
#include <vector>

#include "backtest3/types.hpp"

namespace bt3 {

class VectorReplay {
public:
  VectorReplay(SymbolId sym, std::vector<TickEvent> events)
      : sym_(sym), events_(std::move(events)) {}

  SymbolId sym() const { return sym_; }

  bool has_next(std::size_t idx) const { return idx < events_.size(); }

  const TickEvent& peek(std::size_t idx) const { return events_[idx]; }

  TickEvent next(std::size_t& idx) { return events_[idx++]; }

private:
  SymbolId sym_{};
  std::vector<TickEvent> events_;
};

} // namespace bt3
