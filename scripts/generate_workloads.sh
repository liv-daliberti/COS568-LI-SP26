#! /usr/bin/env bash
# set -e

# echo "Compiling benchmark..."
# git submodule update --init --recursive 

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 8 

function generate_uint64_100M() {
    echo "Generating operations for $1"
    local prefix="../data/$1"
    if [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.000000i" ] &&
       [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.500000i_0m" ] &&
       [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix" ] &&
       [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.900000i_0m_mix_bulkload" ] &&
       [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix" ] &&
       [ -f "${prefix}_ops_2M_0.000000rq_0.500000nl_0.100000i_0m_mix_bulkload" ]; then
        echo "Workloads for $1 already exist, skipping generation."
        return
    fi
    ./generate ../data/$1 2000000 --negative-lookup-ratio 0.5 # generate the lookup workload
    ./generate ../data/$1 2000000 --insert-ratio 0.5 --negative-lookup-ratio 0.5 # generate insert then lookup workload
    ./generate ../data/$1 2000000 --insert-ratio 0.9 --negative-lookup-ratio 0.5 --mix
    ./generate ../data/$1 2000000 --insert-ratio 0.1 --negative-lookup-ratio 0.5 --mix

}


for dataset in fb_100M_public_uint64 books_100M_public_uint64 osmc_100M_public_uint64; do
    generate_uint64_100M $dataset
done
