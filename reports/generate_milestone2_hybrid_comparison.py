from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results"
REPORTS_DIR = ROOT / "reports"

INDEX_ORDER = ["HybridPGMLIPP", "HybridPGMLIPPIncremental"]
INDEX_LABELS = {
    "HybridPGMLIPP": "Naive Hybrid\n(HybridPGMLIPP)",
    "HybridPGMLIPPIncremental": "Incremental Hybrid\n(HybridPGMLIPPIncremental)",
}
INDEX_COLORS = {
    "HybridPGMLIPP": "#1b4965",
    "HybridPGMLIPPIncremental": "#ca6702",
}

WORKLOAD_SPECS = [
    (
        "mix_10_insert",
        "Mixed (90% Lookup / 10% Insert)",
        "fb_100M_public_uint64_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_results_table.csv",
    ),
    (
        "mix_90_insert",
        "Mixed (10% Lookup / 90% Insert)",
        "fb_100M_public_uint64_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv",
    ),
]


def to_gib(size_bytes: float) -> float:
    return size_bytes / (1024 ** 3)


def format_value(value: object) -> str:
    if pd.isna(value):
        return ""
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def split_flush_and_budget(index_name: str, raw_value: object) -> tuple[str, str]:
    if pd.isna(raw_value):
        return "", ""

    text = str(raw_value)
    if index_name == "HybridPGMLIPPIncremental" and "+" in text:
        flush_threshold, drain_budget = text.split("+", 1)
        return flush_threshold, drain_budget

    return text, ""


def format_config(row: pd.Series) -> str:
    parts = []
    if row["search_method"]:
        parts.append(format_value(row["search_method"]))
    if row["value"]:
        parts.append(format_value(row["value"]))
    if row["flush_threshold"]:
        parts.append(f"flush={format_value(row['flush_threshold'])}")
    if row["drain_budget"]:
        parts.append(f"budget={format_value(row['drain_budget'])}")
    return " / ".join(parts)


def select_best_rows() -> pd.DataFrame:
    rows: list[dict[str, object]] = []

    for workload, workload_label, filename in WORKLOAD_SPECS:
        path = RESULTS_DIR / filename
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
            flush_threshold, drain_budget = split_flush_and_budget(
                index_name, best.get("flush_threshold", "")
            )
            row = {
                "workload": workload,
                "workload_label": workload_label,
                "index_name": index_name,
                "index_label": INDEX_LABELS[index_name],
                "avg_mops": float(best["avg_mops"]),
                "index_size_bytes": int(best["index_size_bytes"]),
                "index_size_gib": to_gib(float(best["index_size_bytes"])),
                "search_method": "" if pd.isna(best.get("search_method")) else best["search_method"],
                "value": "" if pd.isna(best.get("value")) else best["value"],
                "flush_threshold": flush_threshold,
                "drain_budget": drain_budget,
            }
            row["config_label"] = format_config(pd.Series(row))
            rows.append(row)

    return pd.DataFrame(rows)


def add_value_labels(ax, suffix: str) -> None:
    top = ax.get_ylim()[1]
    for patch in ax.patches:
        height = patch.get_height()
        ax.text(
            patch.get_x() + patch.get_width() / 2.0,
            height + top * 0.015,
            f"{height:.2f}{suffix}",
            ha="center",
            va="bottom",
            fontsize=9,
        )


def plot_metric(ax, df: pd.DataFrame, workload: str, metric: str, title: str, ylabel: str,
                suffix: str) -> None:
    subset = df[df["workload"] == workload]
    values = [
        subset[subset["index_name"] == index_name].iloc[0][metric]
        for index_name in INDEX_ORDER
    ]
    ax.bar(
        [INDEX_LABELS[index_name] for index_name in INDEX_ORDER],
        values,
        color=[INDEX_COLORS[index_name] for index_name in INDEX_ORDER],
    )
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.grid(axis="y", alpha=0.25)
    add_value_labels(ax, suffix)


def write_summary_table(df: pd.DataFrame, output_path: Path) -> None:
    lines = [
        r"\begin{table}[h]",
        r"\centering",
        r"\scriptsize",
        r"\setlength{\tabcolsep}{4pt}",
        r"\begin{tabular}{p{2.2in}p{1.0in}p{2.2in}}",
        r"\toprule",
        r"Workload & Hybrid Variant & Best Configuration \\",
        r"\midrule",
    ]

    for workload, workload_label, _filename in WORKLOAD_SPECS:
        subset = df[df["workload"] == workload]
        naive = subset[subset["index_name"] == "HybridPGMLIPP"].iloc[0]
        incremental = subset[subset["index_name"] == "HybridPGMLIPPIncremental"].iloc[0]
        lines.append(
            f"{workload_label} & "
            f"\\texttt{{HybridPGMLIPP}}: {naive['avg_mops']:.3f} Mops/s & "
            f"{naive['config_label']} \\\\"
        )
        lines.append(
            f" & \\texttt{{HybridPGMLIPPIncremental}}: {incremental['avg_mops']:.3f} Mops/s & "
            f"{incremental['config_label']} \\\\"
        )
        lines.append(r"\addlinespace")

    if lines[-1] == r"\addlinespace":
        lines.pop()

    lines.extend(
        [
            r"\bottomrule",
            r"\end{tabular}",
            r"\caption{Best Facebook configurations from the supplemental hybrid-only sweep. \texttt{HybridPGMLIPPIncremental} adds a bounded per-insert migration budget in addition to the flush threshold used by \texttt{HybridPGMLIPP}.}",
            r"\end{table}",
        ]
    )

    output_path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", default="fb")
    args = parser.parse_args()

    if args.dataset != "fb":
        raise ValueError("The supplemental hybrid comparison is currently configured for fb only.")

    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    best_df = select_best_rows()
    best_df.to_csv(REPORTS_DIR / "milestone2_hybrid_comparison_fb.csv", index=False)
    write_summary_table(best_df, REPORTS_DIR / "milestone2_hybrid_summary_table.tex")

    fig, axes = plt.subplots(2, 2, figsize=(10.5, 7.0))
    plot_metric(
        axes[0, 0],
        best_df,
        "mix_10_insert",
        "avg_mops",
        "Throughput: 90% Lookup / 10% Insert",
        "Mops/s",
        "",
    )
    plot_metric(
        axes[0, 1],
        best_df,
        "mix_90_insert",
        "avg_mops",
        "Throughput: 10% Lookup / 90% Insert",
        "Mops/s",
        "",
    )
    plot_metric(
        axes[1, 0],
        best_df,
        "mix_10_insert",
        "index_size_gib",
        "Index Size: 90% Lookup / 10% Insert",
        "GiB",
        "",
    )
    plot_metric(
        axes[1, 1],
        best_df,
        "mix_90_insert",
        "index_size_gib",
        "Index Size: 10% Lookup / 90% Insert",
        "GiB",
        "",
    )

    fig.suptitle("Facebook Hybrid Strategy Comparison", fontsize=14)
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(REPORTS_DIR / "milestone2_hybrid_comparison_fb.png", dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    main()
