#!/usr/bin/env python3
"""Generate Milestone 3 report plots and the compact summary table.

The benchmark CSVs are intentionally variable-width because each row appends
the variant parameters after the repeated timing columns.  This script infers
the repeat count from the LIPP row, selects the best mean-throughput row per
index, and writes the 12 required plots.
"""

from __future__ import annotations

from dataclasses import dataclass
import csv
from pathlib import Path
from statistics import mean, stdev

try:
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter
except ModuleNotFoundError as exc:  # pragma: no cover - user-facing dependency hint
    raise SystemExit(
        "matplotlib is required to regenerate the report plots. "
        "Install it with: python3 -m pip install --user matplotlib"
    ) from exc


DATASETS = {
    "books_100M_public_uint64": "Books",
    "fb_100M_public_uint64": "FB",
    "osmc_100M_public_uint64": "OSMC",
}

WORKLOADS = {
    "0.100000i": ("10% Insert", "90% lookup / 10% insert"),
    "0.900000i": ("90% Insert", "10% lookup / 90% insert"),
}

INDEX_LABELS = {
    "DynamicPGM": "DPGM",
    "LIPP": "LIPP",
    "HybridPGMLIPP": "Naive",
    "HybridPGMLIPPAsync": "Async",
}

INDEX_ORDER = ["DynamicPGM", "LIPP", "HybridPGMLIPP", "HybridPGMLIPPAsync"]
COLORS = {
    "DynamicPGM": "#4E79A7",
    "LIPP": "#59A14F",
    "HybridPGMLIPP": "#E15759",
    "HybridPGMLIPPAsync": "#7B61FF",
}


@dataclass(frozen=True)
class ResultRow:
    index: str
    build_times_us: list[float]
    index_size_bytes: float
    throughputs_mops: list[float]
    variant: str

    @property
    def throughput_mean(self) -> float:
        return mean(self.throughputs_mops)

    @property
    def throughput_std(self) -> float:
        return stdev(self.throughputs_mops) if len(self.throughputs_mops) > 1 else 0.0

    @property
    def size_gib(self) -> float:
        return self.index_size_bytes / (1024**3)


def infer_repeats(rows: list[list[str]]) -> int:
    for row in rows:
        if row and row[0] == "LIPP":
            width_without_index = len(row) - 1
            if width_without_index >= 3 and width_without_index % 2 == 1:
                return (width_without_index - 1) // 2
    raise SystemExit("Could not infer repeat count from LIPP row.")


def load_results(path: Path) -> list[ResultRow]:
    with path.open(newline="") as csv_file:
        raw_rows = [
            row for row in csv.reader(csv_file) if row and row[0] != "index_name"
        ]

    repeats = infer_repeats(raw_rows)
    size_col = 1 + repeats
    throughput_start = size_col + 1
    throughput_end = throughput_start + repeats

    parsed: list[ResultRow] = []
    for row in raw_rows:
        if len(row) < throughput_end:
            continue
        parsed.append(
            ResultRow(
                index=row[0],
                build_times_us=[float(x) for x in row[1:size_col]],
                index_size_bytes=float(row[size_col]),
                throughputs_mops=[float(x) for x in row[throughput_start:throughput_end]],
                variant=",".join(row[throughput_end:]),
            )
        )
    return parsed


def best_rows(rows: list[ResultRow]) -> list[ResultRow]:
    best: dict[str, ResultRow] = {}
    for row in rows:
        current = best.get(row.index)
        if current is None or row.throughput_mean > current.throughput_mean:
            best[row.index] = row
    return [best[index] for index in INDEX_ORDER if index in best]


def setup_plot_style() -> None:
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "#FBFBFD",
            "axes.edgecolor": "#D6D8E0",
            "axes.labelcolor": "#20222A",
            "axes.titlecolor": "#20222A",
            "xtick.color": "#20222A",
            "ytick.color": "#20222A",
            "font.family": "DejaVu Sans",
            "font.size": 10,
            "axes.titlesize": 12,
            "axes.titleweight": "bold",
            "axes.labelsize": 10,
            "savefig.facecolor": "white",
            "savefig.edgecolor": "white",
        }
    )


def add_value_labels(ax, bars, values: list[float], fmt: str, pad: float) -> None:
    for bar, value in zip(bars, values):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + pad,
            fmt.format(value),
            ha="center",
            va="bottom",
            fontsize=8.5,
            color="#2B2D33",
        )


def annotate_async_gain(ax, rows: list[ResultRow], ymax: float) -> None:
    by_index = {row.index: row for row in rows}
    lipp = by_index.get("LIPP")
    async_row = by_index.get("HybridPGMLIPPAsync")
    if not lipp or not async_row or lipp.throughput_mean <= 0:
        return
    gain = 100.0 * (async_row.throughput_mean / lipp.throughput_mean - 1.0)
    async_pos = [row.index for row in rows].index("HybridPGMLIPPAsync")
    ax.text(
        async_pos,
        ymax * 0.965,
        f"+{gain:.1f}% vs LIPP",
        ha="center",
        va="top",
        fontsize=8.5,
        fontweight="bold",
        color=COLORS["HybridPGMLIPPAsync"],
        bbox={
            "boxstyle": "round,pad=0.25",
            "facecolor": "white",
            "edgecolor": COLORS["HybridPGMLIPPAsync"],
            "linewidth": 0.8,
        },
    )


def plot_bar(
    rows: list[ResultRow],
    *,
    metric: str,
    title: str,
    subtitle: str,
    output: Path,
) -> None:
    labels = [INDEX_LABELS[row.index] for row in rows]
    colors = [COLORS[row.index] for row in rows]

    if metric == "throughput":
        values = [row.throughput_mean for row in rows]
        errors = [row.throughput_std for row in rows]
        ylabel = "Throughput (Mops/s)"
        value_fmt = "{:.2f}"
        output_pad = max(values) * 0.025
    elif metric == "size":
        values = [row.size_gib for row in rows]
        errors = None
        ylabel = "Index size (GiB)"
        value_fmt = "{:.2f}"
        output_pad = max(values) * 0.025
    else:
        raise ValueError(metric)

    ymax = max(v + (errors[i] if errors else 0.0) for i, v in enumerate(values)) * 1.20

    fig, ax = plt.subplots(figsize=(5.3, 3.35))
    bars = ax.bar(
        labels,
        values,
        yerr=errors,
        capsize=3 if errors else 0,
        color=colors,
        width=0.66,
        edgecolor=["#35577D", "#3D7440", "#A63D40", "#4E39C7"],
        linewidth=[0.8, 0.8, 0.8, 1.5],
        error_kw={"elinewidth": 1.0, "ecolor": "#353844"},
    )

    ax.set_title(title, pad=16)
    ax.text(
        0.5,
        1.01,
        subtitle,
        transform=ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=9,
        color="#5B5F6D",
    )
    ax.set_ylabel(ylabel)
    ax.set_ylim(0, ymax)
    ax.yaxis.grid(True, color="#E4E6EE", linewidth=0.8)
    ax.set_axisbelow(True)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color("#D6D8E0")
    ax.spines["bottom"].set_color("#D6D8E0")
    ax.tick_params(axis="x", length=0)
    ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}"))

    add_value_labels(ax, bars, values, value_fmt, output_pad)
    if metric == "throughput":
        annotate_async_gain(ax, rows, ymax)

    fig.tight_layout(pad=0.8)
    fig.savefig(output, dpi=300, bbox_inches="tight")
    plt.close(fig)


def latex_escape(value: str) -> str:
    return value.replace("_", "\\_")


def variant_label(row: ResultRow) -> str:
    if not row.variant:
        return "-"
    parts = [part for part in row.variant.split(",") if part]
    if len(parts) == 3:
        return f"{parts[0]} / {parts[1]} / {parts[2]}"
    return " / ".join(parts)


def write_summary_table(rows: dict[tuple[str, str], list[ResultRow]], output: Path) -> None:
    lines = [
        "\\begin{table}[h]",
        "\\centering",
        "\\scriptsize",
        "\\setlength{\\tabcolsep}{5pt}",
        "\\begin{tabular}{llrrrr}",
        "\\toprule",
        "Dataset & Workload & LIPP & Async & Gain & Async Variant \\\\",
        "\\midrule",
    ]

    for dataset_token, dataset_label in DATASETS.items():
        first_workload = True
        for workload_token, (workload_label, _) in WORKLOADS.items():
            selected = {
                row.index: row for row in rows[(dataset_token, workload_token)]
            }
            lipp = selected["LIPP"]
            async_row = selected["HybridPGMLIPPAsync"]
            gain = 100.0 * (async_row.throughput_mean / lipp.throughput_mean - 1.0)
            dataset_cell = dataset_label if first_workload else ""
            workload_cell = workload_label.replace("%", "\\%")
            first_workload = False
            lines.append(
                f"{dataset_cell} & {workload_cell} & "
                f"{lipp.throughput_mean:.3f} & {async_row.throughput_mean:.3f} & "
                f"+{gain:.1f}\\% & {latex_escape(variant_label(async_row))} \\\\"
            )
        lines.append("\\addlinespace")

    lines.extend(
        [
            "\\bottomrule",
            "\\end{tabular}",
            "\\caption{Final LIPP-vs-Async comparison from the 3-repeat run. Throughput is in Mops/s.}",
            "\\end{table}",
        ]
    )
    output.write_text("\n".join(lines) + "\n")


def main() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    results_dir = repo_root / "results"
    output_dir = repo_root / "reports" / "milestone3_plots"
    output_dir.mkdir(parents=True, exist_ok=True)
    setup_plot_style()

    selected_by_case: dict[tuple[str, str], list[ResultRow]] = {}
    for dataset_token, dataset_label in DATASETS.items():
        for workload_token, (workload_label, workload_subtitle) in WORKLOADS.items():
            csv_path = (
                results_dir
                / f"{dataset_token}_ops_2M_0.000000rq_0.500000nl_{workload_token}_0m_mix_results_table.csv"
            )
            if not csv_path.exists():
                raise SystemExit(f"Missing results file: {csv_path}")

            selected = best_rows(load_results(csv_path))
            selected_by_case[(dataset_token, workload_token)] = selected

            slug = (
                f"{dataset_label.lower()}_"
                f"{workload_token.replace('.', '').replace('i', 'insert')}"
            )
            plot_title = f"{dataset_label}: {workload_label}"
            plot_bar(
                selected,
                metric="throughput",
                title=plot_title,
                subtitle=workload_subtitle,
                output=output_dir / f"{slug}_throughput.png",
            )
            plot_bar(
                selected,
                metric="size",
                title=plot_title,
                subtitle=workload_subtitle,
                output=output_dir / f"{slug}_index_size.png",
            )

    write_summary_table(
        selected_by_case,
        repo_root / "reports" / "milestone3_summary_table.tex",
    )
    print(f"Wrote polished Milestone 3 plots to {output_dir}")


if __name__ == "__main__":
    main()
