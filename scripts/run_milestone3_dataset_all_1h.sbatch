#!/usr/bin/env bash
#SBATCH --job-name=cos568-m3-dataset
#SBATCH --partition=all
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=64G
#SBATCH --time=01:00:00
#SBATCH --hint=nomultithread
#SBATCH --exclusive
#SBATCH --output=slurm-run_milestone3_%x-%j.out
#SBATCH --error=slurm-run_milestone3_%x-%j.err

set -euo pipefail

if [ -z "${MILESTONE3_DATASET:-}" ]; then
  echo "MILESTONE3_DATASET must be set, e.g. fb_100M_public_uint64" >&2
  exit 2
fi

REPO_DIR="${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$REPO_DIR"

echo "=== COS568 milestone 3 dataset sbatch job ==="
echo "job_id=${SLURM_JOB_ID:-unknown}"
echo "dataset=${MILESTONE3_DATASET}"
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
export MILESTONE3_CHAMPION_ONLY="${MILESTONE3_CHAMPION_ONLY:-1}"
export MILESTONE3_REPEATS="${MILESTONE3_REPEATS:-3}"
export MILESTONE3_THREADS="${MILESTONE3_THREADS:-1}"
export MILESTONE3_DATASETS="${MILESTONE3_DATASET}"
export MILESTONE3_INDEXES="${MILESTONE3_INDEXES:-DynamicPGM LIPP HybridPGMLIPP HybridPGMLIPPAsync}"

for path in \
  "data/${MILESTONE3_DATASET}" \
  "data/${MILESTONE3_DATASET}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix" \
  "data/${MILESTONE3_DATASET}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix" \
  "data/${MILESTONE3_DATASET}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_bulkload" \
  "data/${MILESTONE3_DATASET}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_bulkload"; do
  if [ ! -f "$path" ]; then
    echo "Required file missing: $path" >&2
    exit 1
  fi
done

BUILD_DIR="build_${SLURM_JOB_ID:-manual}_${MILESTONE3_DATASET}"
rm -rf "$BUILD_DIR"
BUILD_DIR="$BUILD_DIR" scripts/build_benchmark.sh
export BENCHMARK="$REPO_DIR/${BUILD_DIR}/benchmark"

mkdir -p results

echo "Running ${MILESTONE3_DATASET} benchmark through srun..."
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK}" --cpu-bind=cores --hint=nomultithread \
  bash scripts/run_milestone3_benchmarks.sh

ARCHIVE_ROOT="$REPO_DIR/sbatch_runs"
RUN_DIR="$ARCHIVE_ROOT/${SLURM_JOB_ID:-manual}-${MILESTONE3_DATASET}-all-1h"
mkdir -p "$RUN_DIR/final/results"
cp -a "results/${MILESTONE3_DATASET}"*_results_table.csv "$RUN_DIR/final/results/" 2>/dev/null || true

echo "end=$(date -Is)"
echo "Artifacts copied to $RUN_DIR/final"
