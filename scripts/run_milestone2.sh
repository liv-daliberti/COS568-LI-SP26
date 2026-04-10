#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

echo "=== Starting Milestone 2 Benchmark ==="

chmod +x scripts/*.sh

echo "Step 1: Downloading datasets..."
./scripts/download_dataset.sh

echo "Step 2: Creating minimal CMakeLists.txt..."
./scripts/create_minimal_cmake.sh

echo "Step 3: Generating workloads..."
./scripts/generate_workloads.sh

echo "Step 4: Building benchmark..."
./scripts/build_benchmark.sh

echo "Step 5: Running milestone 2 benchmarks..."
./scripts/run_milestone2_benchmarks.sh

PYTHON_BIN="python3"
if [ -x "$REPO_DIR/.venv/bin/python" ]; then
    PYTHON_BIN="$REPO_DIR/.venv/bin/python"
fi

REPORT_DATASET="${MILESTONE2_REPORT_DATASET:-fb}"

echo "Step 6: Generating milestone 2 report assets for ${REPORT_DATASET}..."
MPLBACKEND=Agg "$PYTHON_BIN" reports/generate_milestone2_assets.py --report-dataset "$REPORT_DATASET"
MPLBACKEND=Agg "$PYTHON_BIN" reports/generate_milestone2_hybrid_comparison.py --dataset fb

echo "Step 7: Building milestone 2 PDF report..."
(
    cd reports
    pdflatex -interaction=nonstopmode -halt-on-error milestone2_report.tex
    pdflatex -interaction=nonstopmode -halt-on-error milestone2_report.tex
)

echo "=== Milestone 2 benchmark completed successfully ==="
echo "Check results in 'results/' and report assets in 'reports/'."
