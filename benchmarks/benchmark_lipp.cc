#include "benchmarks/benchmark_lipp.h"

#include "benchmark.h"
#include "common.h"
#include "competitors/lipp.h"

void benchmark_64_lipp(tli::Benchmark<uint64_t>& benchmark) {
  benchmark.template Run<Lipp<uint64_t>>();
}

void benchmark_64_lipp_as_hybrid_async(tli::Benchmark<uint64_t>& benchmark) {
  benchmark.template Run<HybridPGMLIPPAsyncLippAlias<uint64_t>>();
}

void benchmark_64_lipp_fast_as_hybrid_async(tli::Benchmark<uint64_t>& benchmark,
                                            const std::string& filename) {
  if (filename.find("0.100000i") != std::string::npos) {
    benchmark.template Run<HybridPGMLIPPAsyncLippFast<uint64_t>>();
  }
}
