#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

echo "=== Starting Milestone 3 Champion Validation ==="

chmod +x scripts/*.sh

echo "Step 1: Downloading datasets..."
./scripts/download_dataset.sh

echo "Step 2: Creating minimal CMakeLists.txt..."
./scripts/create_minimal_cmake.sh

echo "Step 3: Generating workloads..."
./scripts/generate_workloads.sh

echo "Step 4: Building benchmark..."
./scripts/build_benchmark.sh

echo "Step 5: Running write-heavy champion validation benchmarks..."
./scripts/run_milestone3_champion_benchmarks.sh

echo "=== Milestone 3 champion validation completed successfully ==="
echo "Check champion CSVs in 'results_champion/'."
