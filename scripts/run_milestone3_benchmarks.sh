#!/usr/bin/env bash

set -euo pipefail

echo "Executing milestone 3 mixed-workload benchmarks and saving results..."

BENCHMARK="${BENCHMARK:-build/benchmark}"
if [ ! -f "$BENCHMARK" ]; then
    echo "benchmark binary does not exist; run ./scripts/build_benchmark.sh first"
    exit 1
fi

REPEATS="${MILESTONE3_REPEATS:-3}"
THREADS="${MILESTONE3_THREADS:-1}"

execute_mixed_uint64_100M() {
    local dataset="$1"
    local index="$2"

    echo "Executing 90% lookup / 10% insertion workload for ${dataset} on ${index}"
    "$BENCHMARK" "./data/${dataset}" \
        "./data/${dataset}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix" \
        --through --csv --only "${index}" -r "${REPEATS}" --threads "${THREADS}"

    echo "Executing 10% lookup / 90% insertion workload for ${dataset} on ${index}"
    "$BENCHMARK" "./data/${dataset}" \
        "./data/${dataset}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix" \
        --through --csv --only "${index}" -r "${REPEATS}" --threads "${THREADS}"
}

mkdir -p ./results

read -r -a INDEXES <<< "${MILESTONE3_INDEXES:-DynamicPGM LIPP HybridPGMLIPP HybridPGMLIPPAsync}"
read -r -a DATASETS <<< "${MILESTONE3_DATASETS:-fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64}"

for DATA in "${DATASETS[@]}"
do
    rm -f "./results/${DATA}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_results_table.csv"
    rm -f "./results/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv"
done

for DATA in "${DATASETS[@]}"
do
for INDEX in "${INDEXES[@]}"
do
    execute_mixed_uint64_100M "${DATA}" "${INDEX}"
done
done

echo "===================Milestone 3 benchmarking complete!===================="
echo "Generate plots with: python3 reports/generate_milestone3_assets.py"
