#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

echo "=== Starting Milestone 3 Benchmark ==="

chmod +x scripts/*.sh

echo "Step 1: Downloading datasets..."
./scripts/download_dataset.sh

echo "Step 2: Creating minimal CMakeLists.txt..."
./scripts/create_minimal_cmake.sh

echo "Step 2b: Removing stale architecture-specific build artifacts..."
rm -rf build

echo "Step 3: Generating workloads..."
./scripts/generate_workloads.sh

echo "Step 4: Building benchmark..."
./scripts/build_benchmark.sh

echo "Step 5: Running milestone 3 benchmarks..."
./scripts/run_milestone3_benchmarks.sh

PYTHON_BIN="python3"
if [ -x "$REPO_DIR/.venv/bin/python" ]; then
    PYTHON_BIN="$REPO_DIR/.venv/bin/python"
fi

echo "Step 6: Generating milestone 3 plots..."
MPLBACKEND=Agg "$PYTHON_BIN" reports/generate_milestone3_assets.py

echo "Step 7: Building milestone 3 PDF report..."
if command -v pdflatex >/dev/null 2>&1; then
  (
    cd reports
    pdflatex -interaction=nonstopmode -halt-on-error milestone3_report.tex
    pdflatex -interaction=nonstopmode -halt-on-error milestone3_report.tex
  )
else
  echo "pdflatex not found; skipping PDF build."
fi

echo "=== Milestone 3 benchmark completed successfully ==="
echo "Check results in 'results/', plots in 'reports/milestone3_plots/', and report in 'reports/'."
