#!/usr/bin/env python3
import random
import argparse

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="data/sample_ticks_big.csv")
    ap.add_argument("--n", type=int, default=1_000_000)
    ap.add_argument("--start-ts", type=int, default=1700000000000000000)
    ap.add_argument("--dt-ns", type=int, default=1000)  # 1us
    ap.add_argument("--base-price", type=int, default=100_000)
    ap.add_argument("--spread", type=int, default=100)
    args = ap.parse_args()

    ts = args.start_ts
    mid = args.base_price

    with open(args.out, "w") as f:
        f.write("ts_ns,side,price,qty\n")
        for i in range(args.n):
            # small random walk
            mid += random.choice([-1, 0, 1])

            if random.random() < 0.5:
                side = "B"
                price = mid - args.spread // 2
            else:
                side = "A"
                price = mid + args.spread // 2

            qty = random.randint(1, 10)

            f.write(f"{ts},{side},{price},{qty}\n")
            ts += args.dt_ns

    print(f"Generated {args.n} ticks -> {args.out}")

if __name__ == "__main__":
    main()
