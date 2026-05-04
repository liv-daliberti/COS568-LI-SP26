#!/usr/bin/env bash

set -euo pipefail

echo "Executing milestone 3 champion validation benchmarks..."

BENCHMARK=build/benchmark
if [ ! -f "$BENCHMARK" ]; then
    echo "benchmark binary does not exist; run ./scripts/build_benchmark.sh first"
    exit 1
fi

REPEATS="${REPEATS:-5}"

execute_write_heavy_uint64_100M() {
    local dataset="$1"
    local index="$2"

    echo "Executing 10% lookup / 90% insertion workload for ${dataset} on ${index}"
    "$BENCHMARK" "./data/${dataset}" \
        "./data/${dataset}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix" \
        --through --csv --only "${index}" -r "${REPEATS}"
}

mkdir -p ./results ./results_champion

INDEXES=(DynamicPGM LIPP HybridPGMLIPP HybridPGMLIPPAsync)
DATASETS=(fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64)

for DATA in "${DATASETS[@]}"
do
    rm -f "./results/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv"
    rm -f "./results_champion/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv"
done

for DATA in "${DATASETS[@]}"
do
for INDEX in "${INDEXES[@]}"
do
    execute_write_heavy_uint64_100M "${DATA}" "${INDEX}"
done
done

for DATA in "${DATASETS[@]}"
do
    cp "./results/${DATA}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_results_table.csv" \
       "./results_champion/"
done

echo "===================Milestone 3 champion validation complete!===================="
echo "Champion CSVs copied to ./results_champion"
