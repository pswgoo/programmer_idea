#pragma once
#include <cstdint>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "backtest3/replay.hpp"
#include "backtest3/types.hpp"

namespace bt3 {
// A scheduler that returns sym_idx with event (because MarketEvent doesn't include symbol).
  class SymBatchScheduler {
  public:
    using Item = std::pair<std::size_t, q::market::MarketEvent>; // (sym_idx, event)

    explicit SymBatchScheduler(const std::vector<VectorReplay*>& replays,
                               std::int64_t instruments_count)
        : replays_(replays), instruments_count_(instruments_count) {
      init();
    }

    bool has_next() const { return !heap_.empty(); }

    std::vector<Item> next_batch_same_ts() {
      std::vector<Item> batch;
      if (!has_next()) return batch;

      auto first = next_one();
      const auto ts = first.second.ts_ns;
      batch.push_back(first);

      while (has_next()) {
        auto top = heap_.top();
        if (std::get<0>(top) != ts) break;
        batch.push_back(next_one());
      }
      return batch;
    }

  private:
    using Node = std::tuple<std::int64_t, std::int64_t, std::size_t, std::size_t>;
    // (ts, seq, sym_idx, cursor)

    struct Cmp {
      bool operator()(const Node& a, const Node& b) const {
        if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
        if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) > std::get<1>(b);
        return std::get<2>(a) > std::get<2>(b);
      }
    };

    void init() {
      while (!heap_.empty()) heap_.pop();
      for (std::size_t sym_idx = 0; sym_idx < instruments_count_; ++sym_idx) {
        auto* rp = replays_[sym_idx];
        std::size_t cur = 0;
        if (rp->has_next(cur)) {
          const auto& ev = rp->peek(cur);
          heap_.push(Node{ev.ts_ns, ev.seq, sym_idx, cur});
        }
      }
    }

    Item next_one() {
      auto top = heap_.top();
      heap_.pop();

      const auto sym_idx = std::get<2>(top);
      auto cur = std::get<3>(top);
      auto* rp = replays_[sym_idx];
      auto ev = rp->next(cur);

      if (rp->has_next(cur)) {
        const auto& nxt = rp->peek(cur);
        heap_.push(Node{nxt.ts_ns, nxt.seq, sym_idx, cur});
      }
      return {sym_idx, ev};
    }

  private:
    const std::vector<VectorReplay*>& replays_;
    std::int64_t instruments_count_;
    std::priority_queue<Node, std::vector<Node>, Cmp> heap_;
  };

} // namespace bt3
