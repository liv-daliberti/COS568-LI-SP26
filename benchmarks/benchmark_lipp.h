#pragma once
#include "benchmark.h"

void benchmark_64_lipp(tli::Benchmark<uint64_t>& benchmark);
void benchmark_64_lipp_as_hybrid_async(tli::Benchmark<uint64_t>& benchmark);
void benchmark_64_lipp_fast_as_hybrid_async(tli::Benchmark<uint64_t>& benchmark,
                                            const std::string& filename);
