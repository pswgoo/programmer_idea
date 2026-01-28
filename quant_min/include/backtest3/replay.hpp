#pragma once
#include <cstddef>
#include <vector>

#include "market/event.hpp"

namespace bt3 {

class VectorReplay {
public:
  explicit VectorReplay(std::vector<q::market::MarketEvent> events)
      : events_(std::move(events)) {}

  bool has_next(std::size_t idx) const { return idx < events_.size(); }
  const q::market::MarketEvent& peek(std::size_t idx) const { return events_[idx]; }
  q::market::MarketEvent next(std::size_t& idx) { return events_[idx++]; }

private:
  std::vector<q::market::MarketEvent> events_;
};

} // namespace bt3
