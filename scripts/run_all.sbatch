#!/usr/bin/env bash
#SBATCH --job-name=cos568-run-all
#SBATCH --partition=cs
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=64G
#SBATCH --time=04:00:00
#SBATCH --hint=nomultithread
#SBATCH --exclusive
#SBATCH --output=slurm-run_all-%j.out
#SBATCH --error=slurm-run_all-%j.err

set -euo pipefail

REPO_DIR="${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$REPO_DIR"

echo "=== COS568 run_all sbatch job ==="
echo "job_id=${SLURM_JOB_ID:-unknown}"
echo "host=$(hostname)"
echo "start=$(date -Is)"
echo "repo=$REPO_DIR"
echo "cpus_per_task=${SLURM_CPUS_PER_TASK:-unknown}"

export OMP_NUM_THREADS=1
export OMP_PROC_BIND=true
export OMP_PLACES=cores
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1

ARCHIVE_ROOT="$REPO_DIR/sbatch_runs"
RUN_DIR="$ARCHIVE_ROOT/${SLURM_JOB_ID:-manual}"
mkdir -p "$RUN_DIR"

archive_if_present() {
  local path="$1"
  if [ -e "$path" ]; then
    mv "$path" "$RUN_DIR/"
  fi
}

# The benchmark appends to results/*.csv, so archive prior artifacts first.
archive_if_present "$REPO_DIR/results"
archive_if_present "$REPO_DIR/analysis_results"
archive_if_present "$REPO_DIR/benchmark_results.png"

echo "Running scripts/run_all.sh through srun with core binding..."
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK}" --cpu-bind=cores --hint=nomultithread \
  bash scripts/run_all.sh

if [ -x "$REPO_DIR/.venv/bin/python" ]; then
  echo "Running analysis.py inside .venv..."
  . "$REPO_DIR/.venv/bin/activate"
  MPLBACKEND=Agg python scripts/analysis.py
else
  echo "Skipping analysis.py because .venv is missing."
fi

mkdir -p "$RUN_DIR/final"
cp -a "$REPO_DIR/results" "$RUN_DIR/final/" 2>/dev/null || true
cp -a "$REPO_DIR/analysis_results" "$RUN_DIR/final/" 2>/dev/null || true
cp -a "$REPO_DIR/benchmark_results.png" "$RUN_DIR/final/" 2>/dev/null || true

echo "end=$(date -Is)"
echo "Artifacts copied to $RUN_DIR/final"
