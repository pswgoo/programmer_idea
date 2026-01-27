#pragma once
#include <cstdint>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "backtest3/replay.hpp"
#include "backtest3/types.hpp"

namespace bt3 {

class KWayMergeScheduler {
public:
  explicit KWayMergeScheduler(const std::vector<VectorReplay*>& replays)
      : replays_(replays) {
    init_heap();
  }

  bool has_next() const { return !heap_.empty(); }

  TickEvent next() {
    auto top = heap_.top();
    heap_.pop();

    const auto sym = std::get<1>(top);
    auto idx = std::get<2>(top);

    auto* rp = replays_.at(sym_to_replay_index_.at(sym));
    TickEvent ev = rp->next(idx);

    if (rp->has_next(idx)) {
      const auto& nxt = rp->peek(idx);
      heap_.push(Node{nxt.ts_ns, sym, idx});
    }
    return ev;
  }

  // 取同 ts 的 batch（Milestone 2 依赖这个）
  std::vector<TickEvent> next_batch_same_ts() {
    std::vector<TickEvent> batch;
    if (!has_next()) return batch;

    TickEvent first = next();
    const auto ts = first.ts_ns;
    batch.push_back(first);

    while (has_next()) {
      auto top = heap_.top();
      if (std::get<0>(top) != ts) break;
      batch.push_back(next());
    }
    return batch;
  }

private:
  using Node = std::tuple<std::int64_t, SymbolId, std::size_t>; // (ts, sym, idx)
  struct Cmp {
    bool operator()(const Node& a, const Node& b) const {
      if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
      return std::get<1>(a) > std::get<1>(b); // tie: sym
    }
  };

  void init_heap() {
    sym_to_replay_index_.clear();

    for (std::size_t i = 0; i < replays_.size(); ++i) {
      auto* rp = replays_[i];
      sym_to_replay_index_[rp->sym()] = i;

      std::size_t idx = 0;
      if (rp->has_next(idx)) {
        const auto& ev = rp->peek(idx);
        heap_.push(Node{ev.ts_ns, rp->sym(), idx});
      }
    }
  }

private:
  const std::vector<VectorReplay*>& replays_;
  std::priority_queue<Node, std::vector<Node>, Cmp> heap_;
  std::unordered_map<SymbolId, std::size_t> sym_to_replay_index_;
};

} // namespace bt3
