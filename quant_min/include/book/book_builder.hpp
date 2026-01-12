#pragma once
#include <cstdint>
#include <string>

#include "market/event.hpp"

namespace q::book {

enum class BuildState : std::uint8_t {
  NeedSnapshot,
  InSnapshot,
  Live,
  OutOfSync
};

struct BuilderStats {
  std::int64_t last_seq{-1};
  std::size_t gap_count{0};
  std::size_t dup_or_old_count{0};
  std::size_t crossed_count{0};
  std::size_t anomaly_count{0}; // N/C/D 不符合预期等
};

template <class BookT>
class BookBuilder {
public:
  explicit BookBuilder(BookT* book) : book_(book) {}

  BuildState state() const { return state_; }
  const BuilderStats& stats() const { return stats_; }

  // 处理一个 MarketEvent
  void on_event(const q::market::MarketEvent& e) {
    using q::market::Kind;

    switch (e.kind) {
      case Kind::SnapshotBegin:
        on_snapshot_begin(e);
        break;
      case Kind::SnapshotLevel:
        on_snapshot_level(e);
        break;
      case Kind::SnapshotEnd:
        on_snapshot_end(e);
        break;
      case Kind::Incremental:
        on_incremental(e);
        break;
    }
  }

  // 只有 Live 状态才认为 book 可用
  bool book_valid() const { return state_ == BuildState::Live; }

private:
  void on_snapshot_begin(const q::market::MarketEvent& e) {
    // 进入快照：清空 book
    book_->clear();
    snapshot_seq_ = e.seq;
    state_ = BuildState::InSnapshot;
    // snapshot begin 不改 last_seq
  }

  void on_snapshot_level(const q::market::MarketEvent& e) {
    if (state_ != BuildState::InSnapshot) return;
    // 可选：要求 e.seq == snapshot_seq_
    book_->apply_snapshot_level(e.side, e.price, e.qty);
  }

  void on_snapshot_end(const q::market::MarketEvent& e) {
    if (state_ != BuildState::InSnapshot) return;

    // 快照完成：以 end 的 seq 作为 last_seq
    stats_.last_seq = e.seq;
    state_ = BuildState::Live;

    // 可选：做一次 crossed 检查
    check_crossed();
  }

  void on_incremental(const q::market::MarketEvent& e) {
    if (state_ != BuildState::Live) {
      // 没快照或 out-of-sync 时的增量，直接忽略
      return;
    }

    // seq 检查（最严格版本：每条事件 seq 递增 1）
    const auto expected = stats_.last_seq + 1;
    if (e.seq > expected) {
      stats_.gap_count++;
      state_ = BuildState::OutOfSync;
      return;
    }
    if (e.seq <= stats_.last_seq) {
      stats_.dup_or_old_count++;
      return;
    }

    // 应用 N/C/D
    bool ok = book_->apply_incremental(e.side, e.price, e.qty, e.action);
    if (!ok) stats_.anomaly_count++;

    stats_.last_seq = e.seq;

    check_crossed();
  }

  void check_crossed() {
    // 只在 book 可用时检查
    if (state_ != BuildState::Live) return;
    auto top = book_->top();
    if (!top.valid) return;
    if (top.bid_px >= top.ask_px) {
      stats_.crossed_count++;
      // 不一定要 out-of-sync，但要计数（真实系统会报警/触发重建）
    }
  }

private:
    BookT* book_{nullptr};
    BuildState state_{BuildState::NeedSnapshot};
    BuilderStats stats_{};
    std::int64_t snapshot_seq_{-1};
};

} // namespace q::book
