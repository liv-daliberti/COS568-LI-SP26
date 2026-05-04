#!/usr/bin/env python3
"""Fail fast unless the Milestone 3 async hybrid wins the result CSVs.

The benchmark writes variable-width rows:

  index, build_us..., index_size, throughput..., variant...

This checker infers the repeat count from LIPP rows when possible, selects the
best row per index by mean throughput, and exits nonzero if HybridPGMLIPPAsync
does not beat every present baseline for each dataset/workload.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from statistics import mean


DATASETS = [
    "books_100M_public_uint64",
    "fb_100M_public_uint64",
    "osmc_100M_public_uint64",
]

WORKLOADS = {
    "0.100000i": "10% insert",
    "0.900000i": "90% insert",
}

BASELINES = ["DynamicPGM", "LIPP", "HybridPGMLIPP"]
TARGET = "HybridPGMLIPPAsync"


@dataclass
class ResultRow:
    index: str
    throughput: float
    size: int
    variant: str


def infer_repeats(rows: list[list[str]], fallback: int | None) -> int:
    for row in rows:
        if row and row[0] == "LIPP":
            width_without_index = len(row) - 1
            if width_without_index >= 3 and width_without_index % 2 == 1:
                return (width_without_index - 1) // 2
    if fallback is not None:
        return fallback
    raise SystemExit(
        "Could not infer repeat count because no LIPP row was present; pass --repeats."
    )


def parse_rows(path: Path, repeats: int | None) -> list[ResultRow]:
    with path.open(newline="") as handle:
        raw_rows = [
            row for row in csv.reader(handle) if row and row[0] != "index_name"
        ]
    if not raw_rows:
        return []

    repeat_count = infer_repeats(raw_rows, repeats)
    parsed: list[ResultRow] = []
    size_col = 1 + repeat_count
    throughput_start = size_col + 1
    throughput_end = throughput_start + repeat_count

    for row in raw_rows:
        if len(row) < throughput_end:
            continue
        throughputs = [float(x) for x in row[throughput_start:throughput_end]]
        variant = ",".join(row[throughput_end:])
        parsed.append(
            ResultRow(
                index=row[0],
                throughput=mean(throughputs),
                size=int(float(row[size_col])),
                variant=variant,
            )
        )
    return parsed


def best_by_index(rows: list[ResultRow]) -> dict[str, ResultRow]:
    best: dict[str, ResultRow] = {}
    for row in rows:
        current = best.get(row.index)
        if current is None or row.throughput > current.throughput:
            best[row.index] = row
    return best


def check_file(path: Path, repeats: int | None, margin: float) -> bool:
    rows = parse_rows(path, repeats)
    best = best_by_index(rows)
    target = best.get(TARGET)

    print(f"\n{path.name}")
    if target is None:
        print(f"  FAIL: no {TARGET} row")
        return False

    print(
        f"  {TARGET:20s} {target.throughput:8.4f} Mops/s"
        f"  variant={target.variant or '-'}"
    )

    ok = True
    for baseline in BASELINES:
        row = best.get(baseline)
        if row is None:
            continue
        delta = target.throughput - row.throughput
        pct = 100.0 * delta / row.throughput if row.throughput else 0.0
        status = "OK" if delta > margin else "FAIL"
        print(
            f"  vs {baseline:16s} {row.throughput:8.4f} Mops/s"
            f"  delta={delta:+.4f} ({pct:+.2f}%) {status}"
        )
        if delta <= margin:
            ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", default="results")
    parser.add_argument("--repeats", type=int, default=None)
    parser.add_argument(
        "--margin",
        type=float,
        default=0.0,
        help="Required absolute Mops/s margin over each present baseline.",
    )
    args = parser.parse_args()

    root = Path(args.results_dir)
    all_ok = True
    for dataset in DATASETS:
        for token in WORKLOADS:
            path = (
                root
                / f"{dataset}_ops_2M_0.000000rq_0.500000nl_{token}_0m_mix_results_table.csv"
            )
            if not path.exists():
                print(f"\n{path.name}\n  FAIL: missing file")
                all_ok = False
                continue
            all_ok = check_file(path, args.repeats, args.margin) and all_ok

    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
