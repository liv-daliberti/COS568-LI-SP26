#!/usr/bin/env bash

set -euo pipefail

echo "Executing milestone 2 mixed-workload benchmarks and saving results..."

BENCHMARK=build/benchmark
if [ ! -f "$BENCHMARK" ]; then
    echo "benchmark binary does not exist"
    exit 1
fi

execute_mixed_uint64_100M() {
    local dataset="$1"
    local index="$2"

    echo "Executing 90% lookup / 10% insertion workload for ${dataset} on ${index}"
    "$BENCHMARK" "./data/${dataset}" \
        "./data/${dataset}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix" \
        --through --csv --only "${index}" -r 3

    echo "Executing 10% lookup / 90% insertion workload for ${dataset} on ${index}"
    "$BENCHMARK" "./data/${dataset}" \
        "./data/${dataset}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix" \
        --through --csv --only "${index}" -r 3
}

mkdir -p ./results

INDEXES=(DynamicPGM LIPP HybridPGMLIPP)
if [ "${INCLUDE_INCREMENTAL_HYBRID:-0}" = "1" ]; then
    INDEXES+=(HybridPGMLIPPIncremental)
fi

for DATA in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64
do
for INDEX in "${INDEXES[@]}"
do
    execute_mixed_uint64_100M "${DATA}" "${INDEX}"
done
done

echo "===================Milestone 2 benchmarking complete!===================="

shopt -s nullglob
for FILE in ./results/*0.100000i_0m_mix_results_table.csv ./results/*0.900000i_0m_mix_results_table.csv
do
    if head -n 1 "$FILE" | grep -q "index_name"; then
        sed -i '1d' "$FILE"
    fi
    sed -i '1s/^/index_name,build_time_ns1,build_time_ns2,build_time_ns3,index_size_bytes,mixed_throughput_mops1,mixed_throughput_mops2,mixed_throughput_mops3,search_method,value,flush_threshold\n/' "$FILE"
    echo "Header set for $FILE"
done
