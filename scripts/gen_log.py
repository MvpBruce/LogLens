#!/usr/bin/env python3
"""Generate a large, realistic log file to demo/stress-test LogLens.

Usage:
    python scripts/gen_log.py                       # 500k lines -> sample.log
    python scripts/gen_log.py -n 2000000 -o big.log # 2M lines
    python scripts/gen_log.py --seed 42             # reproducible output
    python scripts/gen_log.py --follow live.log     # keep appending (Ctrl+C to stop)

The last form is for demoing live tail: point LogLens at live.log and watch
new lines stream in.

Format per line:
    2026-07-01 12:34:56.789 [LEVEL] component: message [key=value ...]
"""

import argparse
import datetime as dt
import random
import time

# Level distribution roughly like a real service: mostly INFO/DEBUG,
# some WARN, few ERROR. Weights need not sum to 100.
LEVELS = [
    ("DEBUG", 25),
    ("INFO", 50),
    ("WARN", 15),
    ("ERROR", 8),
    ("TRACE", 2),
]

COMPONENTS = [
    "auth", "db", "cache", "http", "scheduler",
    "worker", "config", "metrics", "storage", "api",
]

MESSAGES = {
    "DEBUG": [
        "entering {fn} with args={n}",
        "cache lookup key={key} hit={hit}",
        "query planned in {ms}ms",
    ],
    "TRACE": [
        "span {key} start",
        "heartbeat tick seq={n}",
    ],
    "INFO": [
        "request {method} {path} -> {code} in {ms}ms",
        "user {n} authenticated",
        "job {key} completed in {ms}ms",
        "connection pool size={n}",
    ],
    "WARN": [
        "slow query took {ms}ms (threshold 200ms)",
        "retrying {path} attempt={n}",
        "cache miss rate high: {n}%",
        "deprecated endpoint {path} called",
    ],
    "ERROR": [
        "failed to connect to {path}: timeout after {ms}ms",
        "unhandled exception in {fn}: NullReference",
        "request {method} {path} -> 500 ({ms}ms)",
        "transaction {key} rolled back",
    ],
}

METHODS = ["GET", "POST", "PUT", "DELETE"]
PATHS = ["/api/users", "/api/orders", "/health", "/login", "/api/assets/{id}"]
FUNCS = ["loadScene", "parseGltf", "flushCache", "handleRequest", "runJob"]


def build_line(rng, ts, levels_pool):
    level = rng.choices(
        [l for l, _ in LEVELS], weights=[w for _, w in LEVELS], k=1
    )[0]
    component = rng.choice(COMPONENTS)
    template = rng.choice(MESSAGES[level])
    msg = template.format(
        fn=rng.choice(FUNCS),
        n=rng.randint(1, 9999),
        key=f"{rng.randint(0, 0xffff):04x}",
        hit=rng.choice(["true", "false"]),
        ms=rng.randint(1, 1500),
        method=rng.choice(METHODS),
        path=rng.choice(PATHS).replace("{id}", str(rng.randint(1, 999))),
        code=rng.choice([200, 200, 200, 201, 301, 404]),
    )
    stamp = ts.strftime("%Y-%m-%d %H:%M:%S.") + f"{ts.microsecond // 1000:03d}"
    return f"{stamp} [{level}] {component}: {msg}"


def main():
    ap = argparse.ArgumentParser(description="Generate a demo log file.")
    ap.add_argument("-n", "--lines", type=int, default=500_000,
                    help="number of lines (default 500000)")
    ap.add_argument("-o", "--out", default="sample.log",
                    help="output file (default sample.log)")
    ap.add_argument("--seed", type=int, default=None,
                    help="RNG seed for reproducible output")
    ap.add_argument("--follow", action="store_true",
                    help="keep appending new lines forever (for live tail demo)")
    ap.add_argument("--interval", type=float, default=0.3,
                    help="seconds between lines in --follow mode (default 0.3)")
    args = ap.parse_args()

    rng = random.Random(args.seed)

    if args.follow:
        run_follow(rng, args)
        return

    ts = dt.datetime(2026, 7, 1, 8, 0, 0)
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        for _ in range(args.lines):
            # Advance the clock 1-40ms per line so timestamps look real.
            ts += dt.timedelta(milliseconds=rng.randint(1, 40))
            f.write(build_line(rng, ts, LEVELS))
            f.write("\n")

    print(f"Wrote {args.lines:,} lines to {args.out}")


def run_follow(rng, args):
    """Append one line every --interval seconds until interrupted."""
    print(f"Appending to {args.out} every {args.interval}s -- Ctrl+C to stop")
    n = 0
    try:
        while True:
            with open(args.out, "a", encoding="utf-8", newline="\n") as f:
                f.write(build_line(rng, dt.datetime.now(), LEVELS))
                f.write("\n")
            n += 1
            # Small jitter so the stream doesn't look robotic on screen.
            time.sleep(args.interval * rng.uniform(0.5, 1.5))
    except KeyboardInterrupt:
        print(f"\nStopped after appending {n} lines.")


if __name__ == "__main__":
    main()
