#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

#include "market/event.hpp"

namespace q::book {

struct Level {
  std::int64_t price{};
  std::int64_t qty{};
};

// flat vector L2 book：
// - asks: 价格升序（best ask 在 index 0）
// - bids: 价格降序（best bid 在 index 0）
//
// 更新规则：把 Tick 当作某价位 qty 的“增量”
// qty 累加后 <= 0 则删除该价位

/*
* 1) 降低 cache miss 和指针追逐: std::map 的每个节点分散在堆上，遍历/插入/删除会频繁访问不相邻内存，导致 L1/L2 cache miss。
* 2) 更贴近真实 L2 的工程实现
* 总结：将 L2 book 从 std::map（RB-tree）升级为 cache-friendly 连续容器（flat结构/price ladder），减少指针追逐与动态分配，使回放处理吞吐提升 X%，p99 延迟下降 Y%。
*/
/*
* TODO: 
* 1. 加 taskset/绑核建议到 README，并在 Linux NUMA 机器上跑对比
* 2. 把 flat 的 insert/erase 退化路径优化（比如“热区 vector + 冷区 map”的分层结构）
*/
class FlatL2Book {
public:
  explicit FlatL2Book(std::size_t reserve_levels_per_side = 2048) {
    bids_.reserve(reserve_levels_per_side);
    asks_.reserve(reserve_levels_per_side);
  }

  void clear() {
    bids_.clear();
    asks_.clear();
  }

  // 快照：设置该价位 qty（qty<=0 则删除）
  void apply_snapshot_level(q::market::Side side, std::int64_t price, std::int64_t qty) {
    if (side == q::market::Side::Bid) {
      set_level(bids_, true, price, qty);
    } else if (side == q::market::Side::Ask) {
      set_level(asks_, false, price, qty);
    }
  }

  // 增量：New/Change/Delete
  // 返回 false 表示发现“语义异常”（例如 Delete 不存在、New 已存在等），但仍尽量做鲁棒更新
  bool apply_incremental(q::market::Side side, std::int64_t price, std::int64_t qty, q::market::Action action) {
    bool ok = true;
    if (side != q::market::Side::Bid && side != q::market::Side::Ask) return false;

    auto& vec = (side == q::market::Side::Bid) ? bids_ : asks_;
    const bool is_bid = (side == q::market::Side::Bid);

    // locate
    auto it = lower_bound_price(vec, is_bid, price);
    const bool exists = (it != vec.end() && it->price == price);

    switch (action) {
      case q::market::Action::New:
        if (exists) ok = false; // unexpected
        if (qty > 0) {
          if (exists) it->qty = qty;
          else vec.insert(it, Level{price, qty});
        } else {
          // qty<=0 for New: treat as no-op but mark anomaly
          ok = false;
        }
        break;

      case q::market::Action::Change:
        if (!exists) ok = false; // unexpected
        if (qty <= 0) {
          if (exists) vec.erase(it);
        } else {
          if (exists) it->qty = qty;
          else vec.insert(it, Level{price, qty}); // be robust
        }
        break;

      case q::market::Action::Delete:
        if (!exists) ok = false; // unexpected
        if (exists) vec.erase(it);
        break;

      default:
        return false;
    }
    return ok;
  }

  struct Top {
    std::int64_t bid_px{0}, bid_qty{0};
    std::int64_t ask_px{0}, ask_qty{0};
    bool valid{false};
  };

  Top top() const {
    Top out{};
    if (bids_.empty() || asks_.empty()) return out;
    out.bid_px = bids_[0].price; out.bid_qty = bids_[0].qty;
    out.ask_px = asks_[0].price; out.ask_qty = asks_[0].qty;
    out.valid = true;
    return out;
  }

private:
  static std::vector<Level>::iterator lower_bound_price(std::vector<Level>& side, bool is_bid, std::int64_t price) {
    return std::lower_bound(
      side.begin(), side.end(), price,
      [is_bid](const Level& lv, std::int64_t px) {
        return is_bid ? (lv.price > px) : (lv.price < px); // bids desc, asks asc
      });
  }

  static void set_level(std::vector<Level>& side, bool is_bid, std::int64_t price, std::int64_t qty) {
    auto it = lower_bound_price(side, is_bid, price);
    const bool exists = (it != side.end() && it->price == price);
    if (qty <= 0) {
      if (exists) side.erase(it);
      return;
    }
    if (exists) it->qty = qty;
    else side.insert(it, Level{price, qty});
  }

private:
  std::vector<Level> bids_; // best bid at index 0 (descending by price)
  std::vector<Level> asks_; // best ask at index 0 (ascending by price)
};

} // namespace q::book