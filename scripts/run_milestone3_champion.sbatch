#!/usr/bin/env bash
#SBATCH --job-name=cos568-m3champ
#SBATCH --partition=mltheory
#SBATCH --account=mltheory
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=64G
#SBATCH --time=05:00:00
#SBATCH --hint=nomultithread
#SBATCH --exclusive
#SBATCH --output=slurm-run_milestone3_champion-%j.out
#SBATCH --error=slurm-run_milestone3_champion-%j.err

set -euo pipefail

REPO_DIR="${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$REPO_DIR"

echo "=== COS568 milestone 3 champion validation sbatch job ==="
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
export REPEATS="${REPEATS:-5}"

ARCHIVE_ROOT="$REPO_DIR/sbatch_runs"
RUN_DIR="$ARCHIVE_ROOT/${SLURM_JOB_ID:-manual}-champion"
mkdir -p "$RUN_DIR"

archive_if_present() {
  local path="$1"
  if [ -e "$path" ]; then
    mv "$path" "$RUN_DIR/"
  fi
}

archive_if_present "$REPO_DIR/results_champion"

echo "Running scripts/run_milestone3_champion.sh through srun with core binding..."
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK}" --cpu-bind=cores --hint=nomultithread \
  bash scripts/run_milestone3_champion.sh

mkdir -p "$RUN_DIR/final"
cp -a "$REPO_DIR/results_champion" "$RUN_DIR/final/" 2>/dev/null || true

echo "end=$(date -Is)"
echo "Artifacts copied to $RUN_DIR/final"
