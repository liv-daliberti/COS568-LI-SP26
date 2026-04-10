from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def main() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    analysis_dir = repo_root / "analysis_results"

    mix10 = pd.read_csv(analysis_dir / "insertlookup_mix1_throughput.csv", index_col=0)
    mix90 = pd.read_csv(analysis_dir / "insertlookup_mix2_throughput.csv", index_col=0)

    datasets = ["fb", "books", "osmc"]
    dataset_labels = ["Facebook", "Books", "OSMC"]
    indexes = ["BTree", "DynamicPGM", "LIPP"]
    colors = ["#3b5b92", "#4c956c", "#d17a22"]

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.6), sharey=True)
    bar_width = 0.22
    x = range(len(indexes))

    for ax, frame, title in [
        (axes[0], mix10, "Mixed Workload (10% Insert Ratio)"),
        (axes[1], mix90, "Mixed Workload (90% Insert Ratio)"),
    ]:
        for i, (dataset, label, color) in enumerate(zip(datasets, dataset_labels, colors)):
            values = [frame.loc[dataset, index_name] for index_name in indexes]
            positions = [pos + i * bar_width for pos in x]
            ax.bar(positions, values, width=bar_width, label=label, color=color)

        ax.set_title(title)
        ax.set_ylabel("Throughput (Mops/s)")
        ax.set_xticks([pos + bar_width for pos in x])
        ax.set_xticklabels(indexes)
        ax.grid(axis="y", alpha=0.25)

    axes[1].legend(loc="upper right", frameon=False)
    fig.suptitle("Mixed Insert-Lookup Throughput from the Sbatch Run", fontsize=13)
    plt.tight_layout(rect=[0, 0, 1, 0.95])

    output_path = Path(__file__).resolve().parent / "milestone1_mixed_metrics.png"
    plt.savefig(output_path, dpi=300, bbox_inches="tight")


if __name__ == "__main__":
    main()
