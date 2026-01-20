#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import random
from dataclasses import dataclass
from typing import Dict, Tuple, List, Optional


@dataclass
class BookSide:
    # price -> qty
    levels: Dict[int, int]

    def best_price(self, is_bid: bool) -> Optional[int]:
        if not self.levels:
            return None
        return max(self.levels) if is_bid else min(self.levels)

    def prices_sorted(self, is_bid: bool) -> List[int]:
        return sorted(self.levels.keys(), reverse=is_bid)


class MdEventGen:
    def __init__(
        self,
        seed: int,
        start_ts: int,
        dt_ns: int,
        start_seq: int,
        base_mid: int,
        tick_size: int,
        init_depth: int,
        qty_min: int,
        qty_max: int,
        spread_ticks: int,
    ):
        random.seed(seed)
        self.ts = start_ts
        self.dt = dt_ns
        self.seq = start_seq

        self.tick = tick_size
        self.mid = base_mid
        self.init_depth = init_depth
        self.qty_min = qty_min
        self.qty_max = qty_max
        self.spread_ticks = spread_ticks

        self.bids = BookSide(levels={})
        self.asks = BookSide(levels={})

        self._init_book()

    def _rand_qty(self) -> int:
        return random.randint(self.qty_min, self.qty_max)

    def _init_book(self):
        # Build a symmetric book around mid with a fixed spread.
        # Best bid = mid - spread/2, best ask = mid + spread/2
        half = max(1, self.spread_ticks // 2)
        best_bid = self.mid - half * self.tick
        best_ask = self.mid + half * self.tick

        self.bids.levels.clear()
        self.asks.levels.clear()

        for i in range(self.init_depth):
            px_bid = best_bid - i * self.tick
            px_ask = best_ask + i * self.tick
            self.bids.levels[px_bid] = self._rand_qty()
            self.asks.levels[px_ask] = self._rand_qty()

    def _emit(self, f, kind: str, side: str = "", price: Optional[int] = None, qty: Optional[int] = None, action: str = ""):
        # CSV: ts_ns,seq,kind,side,price,qty,action
        p = "" if price is None else str(price)
        q = "" if qty is None else str(qty)
        f.write(f"{self.ts},{self.seq},{kind},{side},{p},{q},{action}\n")
        self.ts += self.dt

    def emit_snapshot(self, f, max_levels: Optional[int] = None):
        # Use current seq for snapshot block, and use the same seq for SB/SL/SE (simple)
        snap_seq = self.seq
        self._emit(f, "SB")
        # snapshot levels
        levels = max_levels if max_levels is not None else self.init_depth

        # Bid side
        for px in self.bids.prices_sorted(is_bid=True)[:levels]:
            self._emit(f, "SL", "B", px, self.bids.levels[px], "")
        # Ask side
        for px in self.asks.prices_sorted(is_bid=False)[:levels]:
            self._emit(f, "SL", "A", px, self.asks.levels[px], "")
        # End
        self._emit(f, "SE")

        # Keep seq stable inside snapshot, then move to next seq for incrementals
        self.seq = snap_seq + 1

    def _pick_existing_level(self) -> Tuple[str, int]:
        # Choose an existing level from bid or ask (weighted by size)
        choices = []
        if self.bids.levels:
            choices.append("B")
        if self.asks.levels:
            choices.append("A")
        side = random.choice(choices)

        if side == "B":
            px = random.choice(list(self.bids.levels.keys()))
        else:
            px = random.choice(list(self.asks.levels.keys()))
        return side, px

    def _next_new_price(self, side: str) -> int:
        # Create a new price just outside the current book edge
        if side == "B":
            best = self.bids.best_price(is_bid=True)
            if best is None:
                return self.mid - self.tick
            # new bid lower than worst bid (extend depth)
            worst = min(self.bids.levels.keys())
            return worst - self.tick
        else:
            best = self.asks.best_price(is_bid=False)
            if best is None:
                return self.mid + self.tick
            worst = max(self.asks.levels.keys())
            return worst + self.tick

    def _apply_incremental(self, side: str, price: int, qty: int, action: str) -> bool:
        # Apply to internal book model. Return True if semantically expected, False if anomaly.
        book = self.bids.levels if side == "B" else self.asks.levels
        exists = price in book

        ok = True
        if action == "N":
            if exists:
                ok = False
            if qty > 0:
                book[price] = qty  # be robust
            else:
                ok = False
        elif action == "C":
            if not exists:
                ok = False
            if qty <= 0:
                if exists:
                    del book[price]
            else:
                book[price] = qty  # robust set
        elif action == "D":
            if not exists:
                ok = False
            if exists:
                del book[price]
        else:
            ok = False

        return ok

    def emit_incremental_stream(
        self,
        f,
        n_events: int,
        prob_new: float,
        prob_change: float,
        prob_delete: float,
        prob_mid_move: float,
        max_depth_soft: int,
    ):
        # Normalize probs
        total = prob_new + prob_change + prob_delete
        if total <= 0:
            raise ValueError("prob_new/prob_change/prob_delete must sum to > 0")
        prob_new /= total
        prob_change /= total
        prob_delete /= total

        for _ in range(n_events):
            # Occasionally move mid and "recentre" by shifting best prices slightly
            if random.random() < prob_mid_move:
                # random walk by 1 tick
                step = random.choice([-1, 0, 1])
                self.mid += step * self.tick
                # We do NOT rebuild the whole book; instead we let incrementals evolve.
                # This keeps it realistic: book drifts via updates.

            r = random.random()
            if r < prob_new:
                # New: add a new level on either side (typically extending depth)
                side = random.choice(["B", "A"])
                price = self._next_new_price(side)
                qty = self._rand_qty()
                action = "N"

            elif r < prob_new + prob_change:
                # Change: pick existing, modify qty
                side, price = self._pick_existing_level()
                qty = self._rand_qty()
                action = "C"

            else:
                # Delete: pick existing, delete it
                side, price = self._pick_existing_level()
                qty = 0
                action = "D"

            # Soft cap on depth: if too deep, bias towards deletes
            if len(self.bids.levels) > max_depth_soft and random.random() < 0.7:
                # delete a bid
                if self.bids.levels:
                    side = "B"
                    price = random.choice(list(self.bids.levels.keys()))
                    qty = 0
                    action = "D"
            if len(self.asks.levels) > max_depth_soft and random.random() < 0.7:
                if self.asks.levels:
                    side = "A"
                    price = random.choice(list(self.asks.levels.keys()))
                    qty = 0
                    action = "D"

            # Emit event
            self._emit(f, "I", side, price, qty, action)

            # Apply to internal book & advance seq
            self._apply_incremental(side, price, qty, action)
            self.seq += 1

            # Ensure book doesn't go empty on either side (keep it tradable)
            if not self.bids.levels or not self.asks.levels:
                self._init_book()
                # After emergency rebuild, you probably want a snapshot soon in real life,
                # but here we just keep the stream alive.


def main():
    ap = argparse.ArgumentParser(description="Generate snapshot+incremental market event CSV for backtest/LOB builder.")
    ap.add_argument("--out", default="data/sample_md_big.csv")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--events", type=int, default=1_000_000, help="Total incremental events (not counting snapshot lines).")
    ap.add_argument("--snapshot-every", type=int, default=20000, help="Insert a snapshot block every N incrementals.")
    ap.add_argument("--depth", type=int, default=200, help="Snapshot depth per side.")
    ap.add_argument("--max-depth-soft", type=int, default=400, help="Soft cap for book depth during incrementals.")
    ap.add_argument("--base-mid", type=int, default=100_000)
    ap.add_argument("--tick-size", type=int, default=1)
    ap.add_argument("--spread-ticks", type=int, default=2)
    ap.add_argument("--qty-min", type=int, default=1)
    ap.add_argument("--qty-max", type=int, default=20)
    ap.add_argument("--start-ts", type=int, default=1700000000000000000)
    ap.add_argument("--dt-ns", type=int, default=1000, help="Timestamp increment per CSV line.")
    ap.add_argument("--start-seq", type=int, default=100)

    ap.add_argument("--p-new", type=float, default=0.10)
    ap.add_argument("--p-change", type=float, default=0.80)
    ap.add_argument("--p-delete", type=float, default=0.10)
    ap.add_argument("--p-mid-move", type=float, default=0.02, help="Probability of mid moving by 1 tick per incremental.")

    ap.add_argument("--gap-every", type=int, default=0,
                    help="If >0, create a seq gap every N incrementals (for out-of-sync testing).")
    ap.add_argument("--gap-size", type=int, default=1, help="How many seq numbers to skip when creating a gap.")

    args = ap.parse_args()

    # Basic validation
    if args.snapshot_every <= 0:
        raise ValueError("--snapshot-every must be > 0")
    if args.depth <= 0:
        raise ValueError("--depth must be > 0")
    if args.tick_size <= 0:
        raise ValueError("--tick-size must be > 0")
    if args.spread_ticks <= 0:
        raise ValueError("--spread-ticks must be > 0")

    gen = MdEventGen(
        seed=args.seed,
        start_ts=args.start_ts,
        dt_ns=args.dt_ns,
        start_seq=args.start_seq,
        base_mid=args.base_mid,
        tick_size=args.tick_size,
        init_depth=args.depth,
        qty_min=args.qty_min,
        qty_max=args.qty_max,
        spread_ticks=args.spread_ticks,
    )

    with open(args.out, "w", newline="") as f:
        f.write("ts_ns,seq,kind,side,price,qty,action\n")

        # Start with an initial snapshot
        gen.emit_snapshot(f, max_levels=args.depth)

        inc_emitted = 0
        while inc_emitted < args.events:
            # Insert snapshot periodically
            # (In real feeds you might see snapshot only on request; here periodic helps recovery tests.)
            chunk = min(args.snapshot_every, args.events - inc_emitted)

            # Optionally inject a seq gap before generating this chunk
            if args.gap_every > 0 and inc_emitted > 0 and (inc_emitted % args.gap_every) == 0:
                gen.seq += max(1, args.gap_size)

            gen.emit_incremental_stream(
                f,
                n_events=chunk,
                prob_new=args.p_new,
                prob_change=args.p_change,
                prob_delete=args.p_delete,
                prob_mid_move=args.p_mid_move,
                max_depth_soft=args.max_depth_soft,
            )
            inc_emitted += chunk

            # snapshot for resync checkpoints
            if inc_emitted < args.events:
                gen.emit_snapshot(f, max_levels=args.depth)

    print(f"Generated: {args.out}")
    print(f"  incrementals: {args.events}")
    print(f"  snapshot_every: {args.snapshot_every}, depth: {args.depth}")
    if args.gap_every > 0:
        print(f"  gaps: every {args.gap_every} incrementals, gap_size={args.gap_size}")


if __name__ == "__main__":
    main()
