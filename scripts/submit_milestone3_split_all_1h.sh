#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

DATASETS=(
  fb_100M_public_uint64
  books_100M_public_uint64
  osmc_100M_public_uint64
)

for dataset in "${DATASETS[@]}"; do
  short="$dataset"
  short="${short/_100M_public_uint64/}"
  job_id="$(
    sbatch --parsable \
      --job-name="m3-${short}" \
      --export="ALL,MILESTONE3_DATASET=${dataset},MILESTONE3_CHAMPION_ONLY=${MILESTONE3_CHAMPION_ONLY:-1}" \
      "$@" \
      scripts/run_milestone3_dataset_all_1h.sbatch
  )"
  echo "${dataset} ${job_id}"
done
