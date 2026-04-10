#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "benchmarks/common.h"
#include "competitors/hybrid_pgm_lipp.h"

#define RUN_HYBRID(record, searcher, eps, threshold)                                   \
  benchmark.template Run<HybridPGMLIPP<uint64_t, searcher<record>, eps>>(             \
      std::vector<int>{threshold})

#define RUN_HYBRID_INCREMENTAL(record, searcher, eps, threshold, budget)                \
  benchmark                                                                           \
      .template Run<HybridPGMLIPPIncremental<uint64_t, searcher<record>, eps>>(       \
          std::vector<int>{threshold, budget})

template <typename Searcher>
void benchmark_64_hybrid_pgm_lipp(tli::Benchmark<uint64_t>& benchmark, bool pareto,
                                  const std::vector<int>& params) {
  if (!pareto) {
    util::fail("HybridPGMLIPP's hyperparameters cannot be set outside pareto mode");
  }

  const int flush_threshold = params.empty() ? 100000 : std::max(params.front(), 1);
  benchmark.template Run<HybridPGMLIPP<uint64_t, Searcher, 64>>(
      std::vector<int>{flush_threshold});
  benchmark.template Run<HybridPGMLIPP<uint64_t, Searcher, 128>>(
      std::vector<int>{flush_threshold});
  benchmark.template Run<HybridPGMLIPP<uint64_t, Searcher, 512>>(
      std::vector<int>{flush_threshold});
}

template <typename Searcher>
void benchmark_64_hybrid_pgm_lipp_incremental(
    tli::Benchmark<uint64_t>& benchmark, bool pareto,
    const std::vector<int>& params) {
  if (!pareto) {
    util::fail(
        "HybridPGMLIPPIncremental's hyperparameters cannot be set outside "
        "pareto mode");
  }

  const int flush_threshold = params.empty() ? 100000 : std::max(params.front(), 1);
  const int drain_budget = params.size() > 1 ? std::max(params[1], 1) : 256;
  benchmark.template Run<HybridPGMLIPPIncremental<uint64_t, Searcher, 64>>(
      std::vector<int>{flush_threshold, drain_budget});
  benchmark.template Run<HybridPGMLIPPIncremental<uint64_t, Searcher, 128>>(
      std::vector<int>{flush_threshold, drain_budget});
  benchmark.template Run<HybridPGMLIPPIncremental<uint64_t, Searcher, 512>>(
      std::vector<int>{flush_threshold, drain_budget});
}

template <int record>
void benchmark_64_hybrid_pgm_lipp(tli::Benchmark<uint64_t>& benchmark,
                                  const std::string& filename) {
  if (filename.find("fb_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID(record, BranchingBinarySearch, 64, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 100000);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 500000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 500000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 512, 500000);
      return;
    }

    RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
    RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
    RUN_HYBRID(record, BranchingBinarySearch, 512, 100000);
    return;
  }

  if (filename.find("books_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID(record, LinearSearch, 32, 25000);
      RUN_HYBRID(record, LinearSearch, 32, 50000);
      RUN_HYBRID(record, LinearSearch, 32, 100000);
      RUN_HYBRID(record, InterpolationSearch, 128, 25000);
      RUN_HYBRID(record, InterpolationSearch, 128, 50000);
      RUN_HYBRID(record, InterpolationSearch, 128, 100000);
      RUN_HYBRID(record, InterpolationSearch, 256, 25000);
      RUN_HYBRID(record, InterpolationSearch, 256, 50000);
      RUN_HYBRID(record, InterpolationSearch, 256, 100000);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID(record, LinearSearch, 32, 100000);
      RUN_HYBRID(record, LinearSearch, 32, 250000);
      RUN_HYBRID(record, LinearSearch, 32, 500000);
      RUN_HYBRID(record, InterpolationSearch, 128, 100000);
      RUN_HYBRID(record, InterpolationSearch, 128, 250000);
      RUN_HYBRID(record, InterpolationSearch, 128, 500000);
      RUN_HYBRID(record, InterpolationSearch, 256, 100000);
      RUN_HYBRID(record, InterpolationSearch, 256, 250000);
      RUN_HYBRID(record, InterpolationSearch, 256, 500000);
      return;
    }

    RUN_HYBRID(record, LinearSearch, 32, 100000);
    RUN_HYBRID(record, InterpolationSearch, 128, 100000);
    RUN_HYBRID(record, InterpolationSearch, 256, 100000);
    return;
  }

  if (filename.find("osmc_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID(record, BranchingBinarySearch, 64, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 25000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 50000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 100000);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 64, 500000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 128, 500000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 100000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 250000);
      RUN_HYBRID(record, BranchingBinarySearch, 256, 500000);
      return;
    }

    RUN_HYBRID(record, BranchingBinarySearch, 64, 100000);
    RUN_HYBRID(record, BranchingBinarySearch, 128, 100000);
    RUN_HYBRID(record, BranchingBinarySearch, 256, 100000);
  }
}

template <int record>
void benchmark_64_hybrid_pgm_lipp_incremental(
    tli::Benchmark<uint64_t>& benchmark, const std::string& filename) {
  if (filename.find("fb_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 100000, 256);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 500000, 1024);
      return;
    }

    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 512, 100000, 256);
    return;
  }

  if (filename.find("books_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 100000, 256);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 500000, 1024);
      return;
    }

    RUN_HYBRID_INCREMENTAL(record, LinearSearch, 32, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 128, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, InterpolationSearch, 256, 100000, 256);
    return;
  }

  if (filename.find("osmc_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 25000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 25000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 100000, 64);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 100000, 256);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 100000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 100000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 500000, 1024);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 500000, 256);
      RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 500000, 1024);
      return;
    }

    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 64, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 128, 100000, 256);
    RUN_HYBRID_INCREMENTAL(record, BranchingBinarySearch, 256, 100000, 256);
  }
}

#undef RUN_HYBRID
#undef RUN_HYBRID_INCREMENTAL

INSTANTIATE_TEMPLATES_MULTITHREAD(benchmark_64_hybrid_pgm_lipp, uint64_t);
INSTANTIATE_TEMPLATES_MULTITHREAD(benchmark_64_hybrid_pgm_lipp_incremental,
                                  uint64_t);
