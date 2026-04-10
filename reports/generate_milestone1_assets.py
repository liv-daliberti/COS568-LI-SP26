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

INDEX_ORDER = ["BTree", "DynamicPGM", "LIPP"]
INDEX_LABELS = {
    "BTree": "B+Tree",
    "DynamicPGM": "Dynamic PGM",
    "LIPP": "LIPP",
}
INDEX_COLORS = {
    "BTree": "#1b4965",
    "DynamicPGM": "#ca6702",
    "LIPP": "#2a9d8f",
}


def select_best_rows():
    rows = []
    for dataset in ["fb", "books", "osmc"]:
        full_name = f"{dataset}_100M_public_uint64"
        specs = [
            (
                "lookup_only",
                RESULTS_DIR
                / f"{full_name}_ops_2M_0.000000rq_0.500000nl_0.000000i_results_table.csv",
                "lookup_throughput_mops",
            ),
            (
                "insert_lookup_insert",
                RESULTS_DIR
                / f"{full_name}_ops_2M_0.000000rq_0.500000nl_0.500000i_0m_results_table.csv",
                "insert_throughput_mops",
            ),
            (
                "insert_lookup_lookup",
                RESULTS_DIR
                / f"{full_name}_ops_2M_0.000000rq_0.500000nl_0.500000i_0m_results_table.csv",
                "lookup_throughput_mops",
            ),
        ]

        for workload, path, prefix in specs:
            df = pd.read_csv(path)
            for index_name in INDEX_ORDER:
                sub = df[df["index_name"] == index_name].copy()
                metric_cols = [c for c in sub.columns if c.startswith(prefix)]
                sub["avg_mops"] = sub[metric_cols].mean(axis=1)
                best = sub.sort_values("avg_mops", ascending=False).iloc[0]
                rows.append(
                    {
                        "dataset": dataset,
                        "dataset_label": DATASET_LABELS[dataset],
                        "workload": workload,
                        "index_name": index_name,
                        "index_label": INDEX_LABELS[index_name],
                        "avg_mops": float(best["avg_mops"]),
                        "search_method": (
                            "" if pd.isna(best.get("search_method")) else best["search_method"]
                        ),
                        "value": "" if pd.isna(best.get("value")) else best["value"],
                    }
                )
    return pd.DataFrame(rows)


def plot_metric(ax, df, workload, title, log_scale=False):
    subset = df[df["workload"] == workload]
    datasets = list(DATASET_LABELS.values())
    x = range(len(datasets))
    width = 0.22

    for offset, index_name in enumerate(INDEX_ORDER):
        values = []
        for dataset in ["fb", "books", "osmc"]:
            row = subset[
                (subset["dataset"] == dataset) & (subset["index_name"] == index_name)
            ].iloc[0]
            values.append(row["avg_mops"])

        positions = [i + (offset - 1) * width for i in x]
        ax.bar(
            positions,
            values,
            width=width,
            label=INDEX_LABELS[index_name],
            color=INDEX_COLORS[index_name],
        )

    ax.set_title(title)
    ax.set_xticks(list(x))
    ax.set_xticklabels(datasets)
    ax.set_ylabel("Mops/s")
    if log_scale:
        ax.set_yscale("log")
    ax.grid(axis="y", alpha=0.25)


def main():
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    best_df = select_best_rows()
    best_df.to_csv(REPORTS_DIR / "milestone1_best_variants.csv", index=False)

    fig, axes = plt.subplots(1, 3, figsize=(13.5, 3.9))
    plot_metric(axes[0], best_df, "lookup_only", "Lookup-Only Throughput", log_scale=True)
    plot_metric(
        axes[1], best_df, "insert_lookup_insert", "Insert Throughput (50/50 Workload)"
    )
    plot_metric(
        axes[2],
        best_df,
        "insert_lookup_lookup",
        "Lookup After Insertion (50/50 Workload)",
        log_scale=True,
    )
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=3, frameon=False)
    fig.tight_layout(rect=[0, 0, 1, 0.9])
    fig.savefig(REPORTS_DIR / "milestone1_metrics.png", dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    main()
