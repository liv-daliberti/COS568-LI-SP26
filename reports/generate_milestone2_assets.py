from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results"
REPORTS_DIR = ROOT / "reports"

DATASET_LABELS = {
    "fb": "Facebook",
    "books": "Books",
    "osmc": "OSMC",
}

INDEX_ORDER = ["DynamicPGM", "LIPP", "HybridPGMLIPP"]
INDEX_LABELS = {
    "DynamicPGM": "Dynamic PGM",
    "LIPP": "LIPP",
    "HybridPGMLIPP": "Hybrid",
}
INDEX_COLORS = {
    "DynamicPGM": "#ca6702",
    "LIPP": "#2a9d8f",
    "HybridPGMLIPP": "#1b4965",
}

WORKLOAD_SPECS = [
    (
        "mix_10_insert",
        "Mixed (90% Lookup / 10% Insert)",
        "_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_results_table.csv",
    ),
    (
        "mix_90_insert",
        "Mixed (10% Lookup / 90% Insert)",
        "_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv",
    ),
]


def to_gib(size_bytes: float) -> float:
    return size_bytes / (1024 ** 3)


def format_variant_value(value: object) -> str:
    if pd.isna(value):
        return ""
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def select_best_rows(datasets: list[str]) -> pd.DataFrame:
    rows: list[dict[str, object]] = []

    for dataset in datasets:
        dataset_label = DATASET_LABELS[dataset]
        full_name = f"{dataset}_100M_public_uint64"
        for workload, workload_label, suffix in WORKLOAD_SPECS:
            path = RESULTS_DIR / f"{full_name}{suffix}"
            if not path.exists():
                raise FileNotFoundError(f"Missing results file: {path}")

            df = pd.read_csv(path)
            metric_cols = [c for c in df.columns if c.startswith("mixed_throughput_mops")]
            if not metric_cols:
                raise ValueError(f"No mixed throughput columns found in {path}")

            for index_name in INDEX_ORDER:
                sub = df[df["index_name"] == index_name].copy()
                if sub.empty:
                    raise ValueError(f"No rows found for {index_name} in {path}")

                sub["avg_mops"] = sub[metric_cols].mean(axis=1)
                best = sub.sort_values("avg_mops", ascending=False).iloc[0]
                rows.append(
                    {
                        "dataset": dataset,
                        "dataset_label": dataset_label,
                        "workload": workload,
                        "workload_label": workload_label,
                        "index_name": index_name,
                        "index_label": INDEX_LABELS[index_name],
                        "avg_mops": float(best["avg_mops"]),
                        "index_size_bytes": int(best["index_size_bytes"]),
                        "index_size_gib": to_gib(float(best["index_size_bytes"])),
                        "search_method": best.get("search_method", ""),
                        "value": best.get("value", ""),
                        "flush_threshold": best.get("flush_threshold", ""),
                    }
                )

    return pd.DataFrame(rows)


def plot_metric(ax, df: pd.DataFrame, workload: str, metric: str, title: str, ylabel: str) -> None:
    subset = df[df["workload"] == workload]
    values = []
    for index_name in INDEX_ORDER:
        row = subset[subset["index_name"] == index_name].iloc[0]
        values.append(row[metric])

    ax.bar(
        [INDEX_LABELS[index_name] for index_name in INDEX_ORDER],
        values,
        color=[INDEX_COLORS[index_name] for index_name in INDEX_ORDER],
    )
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.grid(axis="y", alpha=0.25)


def write_summary_table(report_df: pd.DataFrame, output_path: Path) -> None:
    lines = [
        r"\begin{table}[h]",
        r"\centering",
        r"\scriptsize",
        r"\setlength{\tabcolsep}{4pt}",
        r"\begin{tabular}{llrrrr}",
        r"\toprule",
        r"Workload & Index & Throughput (Mops/s) & Size (GiB) & Search & Flush \\",
        r"\midrule",
    ]

    for workload, workload_label, _suffix in WORKLOAD_SPECS:
        subset = report_df[report_df["workload"] == workload]
        for index_name in INDEX_ORDER:
            row = subset[subset["index_name"] == index_name].iloc[0]
            search = format_variant_value(row["search_method"])
            value = format_variant_value(row["value"])
            flush_threshold = format_variant_value(row["flush_threshold"])
            if search and value:
                search = f"{search} / {value}"
            flush_text = flush_threshold if flush_threshold else "-"
            index_label = INDEX_LABELS[index_name]
            workload_cell = workload_label if index_name == INDEX_ORDER[0] else ""
            lines.append(
                f"{workload_cell} & {index_label} & {row['avg_mops']:.3f} & "
                f"{row['index_size_gib']:.3f} & {search or '-'} & {flush_text} \\\\"
            )
        lines.append(r"\addlinespace")

    if lines[-1] == r"\addlinespace":
        lines.pop()

    lines.extend(
        [
            r"\bottomrule",
            r"\end{tabular}",
            r"\caption{Best mean-performing configuration per index for the reported Facebook mixed workloads. Dynamic PGM and the hybrid are selected after averaging three repeats and sweeping their candidate hyperparameters.}",
            r"\end{table}",
        ]
    )

    output_path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report-dataset", default="fb", choices=sorted(DATASET_LABELS))
    parser.add_argument(
        "--datasets",
        nargs="+",
        default=sorted(DATASET_LABELS),
        choices=sorted(DATASET_LABELS),
        help="Datasets to include when selecting best rows and generating summary assets.",
    )
    args = parser.parse_args()

    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    best_df = select_best_rows(args.datasets)

    report_df = best_df[best_df["dataset"] == args.report_dataset].copy()
    if report_df.empty:
        raise ValueError(f"No rows found for report dataset '{args.report_dataset}'")

    best_df.to_csv(REPORTS_DIR / "milestone2_best_variants_all.csv", index=False)
    report_df.to_csv(REPORTS_DIR / "milestone2_best_variants_fb.csv", index=False)

    fig, axes = plt.subplots(2, 2, figsize=(10.5, 7.2))
    plot_metric(
        axes[0, 0],
        report_df,
        "mix_10_insert",
        "avg_mops",
        "Throughput: 90% Lookup / 10% Insert",
        "Mops/s",
    )
    plot_metric(
        axes[0, 1],
        report_df,
        "mix_10_insert",
        "index_size_gib",
        "Index Size: 90% Lookup / 10% Insert",
        "GiB",
    )
    plot_metric(
        axes[1, 0],
        report_df,
        "mix_90_insert",
        "avg_mops",
        "Throughput: 10% Lookup / 90% Insert",
        "Mops/s",
    )
    plot_metric(
        axes[1, 1],
        report_df,
        "mix_90_insert",
        "index_size_gib",
        "Index Size: 10% Lookup / 90% Insert",
        "GiB",
    )

    fig.suptitle(
        f"Milestone 2 Mixed-Workload Results ({DATASET_LABELS[args.report_dataset]})",
        fontsize=14,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(REPORTS_DIR / "milestone2_metrics.png", dpi=300, bbox_inches="tight")

    write_summary_table(report_df, REPORTS_DIR / "milestone2_summary_table.tex")


if __name__ == "__main__":
    main()
