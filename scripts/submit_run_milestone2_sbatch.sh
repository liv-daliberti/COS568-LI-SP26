#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

JOB_ID="$(sbatch --parsable "$@" scripts/run_milestone2.sbatch)"
echo "$JOB_ID"
