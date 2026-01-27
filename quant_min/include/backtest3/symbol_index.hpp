#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "backtest3/types.hpp"

namespace bt3 {

// 把 SymbolId 映射到 [0..N) 的连续 index，便于用 vector 存状态
class SymbolIndex {
public:
  explicit SymbolIndex(const std::vector<Instrument>& instruments) {
    id_to_idx_.reserve(instruments.size());
    idx_to_id_.reserve(instruments.size());
    for (std::size_t i = 0; i < instruments.size(); ++i) {
      const auto id = instruments[i].id;
      id_to_idx_[id] = i;
      idx_to_id_.push_back(id);
    }
  }

  std::size_t idx(SymbolId id) const {
    return id_to_idx_.at(id);
  }

  SymbolId id(std::size_t idx) const {
    return idx_to_id_.at(idx);
  }

  std::size_t size() const { return idx_to_id_.size(); }

private:
  std::unordered_map<SymbolId, std::size_t> id_to_idx_;
  std::vector<SymbolId> idx_to_id_;
};

} // namespace bt3
