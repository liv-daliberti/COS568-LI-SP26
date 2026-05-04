#include "benchmarks/benchmark_hybrid_pgm_lipp.h"

#include "benchmark.h"
#include "benchmarks/common.h"
#include "competitors/hybrid_pgm_lipp.h"

#include <algorithm>
#include <cstdlib>

// Keep the submitted binary focused on the final configurations. The old
// exploratory sweeps remain in the source below for provenance, but compiling
// every experimental template costs several minutes per sbatch job.
namespace {
static constexpr bool kCompileChampionOnly = true;

}  // namespace

#define RUN_HYBRID(record, searcher, eps, threshold)                                   \
  benchmark.template Run<HybridPGMLIPP<uint64_t, searcher<record>, eps>>(             \
      std::vector<int>{threshold})

#define RUN_HYBRID_INCREMENTAL(record, searcher, eps, threshold, budget)                \
  benchmark                                                                           \
      .template Run<HybridPGMLIPPIncremental<uint64_t, searcher<record>, eps>>(       \
          std::vector<int>{threshold, budget})

#define RUN_HYBRID_ASYNC(record, searcher, eps, threshold)                              \
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, searcher<record>, eps>>(        \
      std::vector<int>{threshold})

#define RUN_HYBRID_ASYNC_MODE(record, searcher, eps, threshold, mode)                   \
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, searcher<record>, eps>>(        \
      std::vector<int>{threshold, mode})

#define RUN_HYBRID_ASYNC_MODE_SHARDS(record, searcher, eps, threshold, mode, shards)   \
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, searcher<record>, eps>>(        \
      std::vector<int>{threshold, mode, shards})

#define RUN_HYBRID_ASYNC_EXACT(record, searcher, eps)                                   \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPAsyncExactLIPP<uint64_t, searcher<record>, eps>>()

#define RUN_HYBRID_ASYNC_EXACT_LOOKUP(record, searcher, eps, mode)                     \
  benchmark                                                                            \
      .template Run<                                                                  \
          HybridPGMLIPPAsyncExactLIPPVariant<uint64_t, searcher<record>, eps, mode>>()

#define RUN_HYBRID_ASYNC_STOCK_LIPP(record, searcher)                                  \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPAsyncStockLIPP<uint64_t, searcher<record>>>()

#define RUN_HYBRID_ASYNC_SLACK_LIPP(record, searcher, eps, slack_ppm)                  \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPSlackLIPP<uint64_t, searcher<record>, eps>>(         \
          std::vector<int>{slack_ppm})

#define RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, searcher, eps, slack_ppm,            \
                                           rebuild_factor, collision_inv)              \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPSlackLIPP<uint64_t, searcher<record>, eps>>(         \
          std::vector<int>{slack_ppm, rebuild_factor, collision_inv})

#define RUN_HYBRID_ASYNC_TUNED_LIPP(record, searcher, eps, rebuild_factor,             \
                                    collision_inv)                                     \
  RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, searcher, eps, 0, rebuild_factor,          \
                                    collision_inv)

#define RUN_HYBRID_ASYNC_PARTITIONED(record, searcher, eps, shards)                    \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPPartitionedDelta<uint64_t, searcher<record>, eps>>(  \
          std::vector<int>{shards})

#define RUN_HYBRID_ASYNC_LIPP_DELTA(record, searcher, eps, shards)                    \
  benchmark                                                                            \
      .template Run<                                                                 \
          HybridPGMLIPPPartitionedLippDelta<uint64_t, searcher<record>, eps>>(       \
          std::vector<int>{shards})

#define RUN_HYBRID_ASYNC_SHARDED_LIPP(record, searcher, eps, shards)                  \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPShardedLIPP<uint64_t, searcher<record>, eps>>(      \
          std::vector<int>{shards})

#define RUN_HYBRID_ASYNC_GUARDED_DELTA(record, searcher, eps, buckets)                \
  benchmark                                                                            \
      .template Run<HybridPGMLIPPGuardedDelta<uint64_t, searcher<record>, eps>>(     \
          std::vector<int>{buckets})

template <typename Searcher>
void benchmark_64_hybrid_pgm_lipp(tli::Benchmark<uint64_t>& benchmark, bool pareto,
                                  const std::vector<int>& params) {
  if constexpr (kCompileChampionOnly) {
    (void) benchmark;
    (void) pareto;
    (void) params;
    return;
  }

#if 0
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
#endif
}

template <typename Searcher>
void benchmark_64_hybrid_pgm_lipp_incremental(
    tli::Benchmark<uint64_t>& benchmark, bool pareto,
    const std::vector<int>& params) {
  if constexpr (kCompileChampionOnly) {
    (void) benchmark;
    (void) pareto;
    (void) params;
    return;
  }

#if 0
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
#endif
}

template <typename Searcher>
void benchmark_64_hybrid_pgm_lipp_async(
    tli::Benchmark<uint64_t>& benchmark, bool pareto,
    const std::vector<int>& params) {
  if constexpr (kCompileChampionOnly) {
    (void) benchmark;
    (void) pareto;
    (void) params;
    return;
  }

#if 0
  if (!pareto) {
    util::fail(
        "HybridPGMLIPPAsync's hyperparameters cannot be set outside pareto "
        "mode");
  }

  const int flush_threshold = params.empty() ? 100000 : std::max(params.front(), 1);
  const int probe_mode = params.size() > 1 ? std::clamp(params[1], 0, 3) : 0;
  const int partitioned_shards = params.size() > 2 ? std::max(params[2], 0) : 0;
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, Searcher, 64>>(
      std::vector<int>{flush_threshold, probe_mode, partitioned_shards});
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, Searcher, 128>>(
      std::vector<int>{flush_threshold, probe_mode, partitioned_shards});
  benchmark.template Run<HybridPGMLIPPAsync<uint64_t, Searcher, 512>>(
      std::vector<int>{flush_threshold, probe_mode, partitioned_shards});
#endif
}

template <int record>
void benchmark_64_hybrid_pgm_lipp(tli::Benchmark<uint64_t>& benchmark,
                                  const std::string& filename) {
  if constexpr (kCompileChampionOnly) {
    if (filename.find("fb_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        RUN_HYBRID(record, BranchingBinarySearch, 512, 25000);
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID(record, BranchingBinarySearch, 128, 500000);
        return;
      }
    }
    if (filename.find("books_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        RUN_HYBRID(record, InterpolationSearch, 256, 25000);
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID(record, InterpolationSearch, 256, 500000);
        return;
      }
    }
    if (filename.find("osmc_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        RUN_HYBRID(record, BranchingBinarySearch, 128, 25000);
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID(record, BranchingBinarySearch, 128, 500000);
        return;
      }
    }
    return;
  }

#if 0
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
#endif
}

template <int record>
void benchmark_64_hybrid_pgm_lipp_incremental(
    tli::Benchmark<uint64_t>& benchmark, const std::string& filename) {
  if constexpr (kCompileChampionOnly) {
    (void) benchmark;
    (void) filename;
    return;
  }

#if 0
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
#endif
}

template <int record>
void benchmark_64_hybrid_pgm_lipp_async(
    tli::Benchmark<uint64_t>& benchmark, const std::string& filename) {
  if constexpr (kCompileChampionOnly) {
    if (filename.find("fb_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        // Read-heavy champion is emitted from benchmark_lipp.cc as
        // stock_lipp_fast_tu so it is compiled in the same TU as LIPP.
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 5000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 5000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 8000000, 2);
        return;
      }
    }
    if (filename.find("books_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        // Read-heavy champion is emitted from benchmark_lipp.cc as
        // stock_lipp_fast_tu so it is compiled in the same TU as LIPP.
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 2000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 3000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 5000000, 2);
        return;
      }
    }
    if (filename.find("osmc_100M") != std::string::npos) {
      if (filename.find("0.100000i") != std::string::npos) {
        // Read-heavy champion is emitted from benchmark_lipp.cc as
        // stock_lipp_fast_tu so it is compiled in the same TU as LIPP.
        return;
      }
      if (filename.find("0.900000i") != std::string::npos) {
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 2000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 5000000, 2);
        RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 256, 5000000, 2);
        return;
      }
    }
    return;
  }

#if 0
  if (filename.find("fb_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_EXACT(record, BranchingBinarySearch, 64);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 1000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 2500);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 5000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 10000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 20000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 50000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 64, 100000);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 64, 1000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 64, 5000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 64, 20000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 64, 5000,
                                        1000000, 1);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 8000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 512, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 512, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 512, 8000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 512, 12000000, 2);
      return;
    }

    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 64, 100000);
    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 128, 100000);
    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 512, 100000);
    return;
  }

  if (filename.find("books_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_EXACT(record, LinearSearch, 32);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, LinearSearch, 32, 1000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, LinearSearch, 32, 2500);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, LinearSearch, 32, 5000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, LinearSearch, 32, 10000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, LinearSearch, 32, 50000);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, LinearSearch, 32, 5000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, LinearSearch, 32, 10000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, LinearSearch, 32, 50000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, LinearSearch, 32, 10000, 1000000,
                                        1);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_MODE(record, LinearSearch, 32, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, LinearSearch, 32, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 128, 1000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 128, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 128, 3000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 1000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 3000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, InterpolationSearch, 256, 12000000, 2);
      return;
    }

    RUN_HYBRID_ASYNC(record, LinearSearch, 32, 100000);
    RUN_HYBRID_ASYNC(record, InterpolationSearch, 128, 100000);
    RUN_HYBRID_ASYNC(record, InterpolationSearch, 256, 100000);
    return;
  }

  if (filename.find("osmc_100M") != std::string::npos) {
    if (filename.find("0.100000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_EXACT(record, BranchingBinarySearch, 128);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 128, 1000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 128, 2500);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 128, 5000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 128, 10000);
      RUN_HYBRID_ASYNC_SLACK_LIPP(record, BranchingBinarySearch, 128, 50000);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 128, 1000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 128, 10000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 128, 50000, 8, 2);
      RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED(record, BranchingBinarySearch, 128, 1000,
                                        1000000, 1);
      return;
    }
    if (filename.find("0.900000i") != std::string::npos) {
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 1000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 64, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 1000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 3000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 128, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 256, 2000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 256, 5000000, 2);
      RUN_HYBRID_ASYNC_MODE(record, BranchingBinarySearch, 256, 12000000, 2);
      return;
    }

    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 64, 100000);
    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 128, 100000);
    RUN_HYBRID_ASYNC(record, BranchingBinarySearch, 256, 100000);
  }
#endif
}

#undef RUN_HYBRID
#undef RUN_HYBRID_INCREMENTAL
#undef RUN_HYBRID_ASYNC
#undef RUN_HYBRID_ASYNC_MODE
#undef RUN_HYBRID_ASYNC_MODE_SHARDS
#undef RUN_HYBRID_ASYNC_EXACT
#undef RUN_HYBRID_ASYNC_EXACT_LOOKUP
#undef RUN_HYBRID_ASYNC_STOCK_LIPP
#undef RUN_HYBRID_ASYNC_SLACK_LIPP
#undef RUN_HYBRID_ASYNC_SLACK_LIPP_TUNED
#undef RUN_HYBRID_ASYNC_TUNED_LIPP
#undef RUN_HYBRID_ASYNC_PARTITIONED
#undef RUN_HYBRID_ASYNC_LIPP_DELTA
#undef RUN_HYBRID_ASYNC_SHARDED_LIPP
#undef RUN_HYBRID_ASYNC_GUARDED_DELTA

INSTANTIATE_TEMPLATES_MULTITHREAD(benchmark_64_hybrid_pgm_lipp, uint64_t);
INSTANTIATE_TEMPLATES_MULTITHREAD(benchmark_64_hybrid_pgm_lipp_incremental,
                                  uint64_t);
INSTANTIATE_TEMPLATES_MULTITHREAD(benchmark_64_hybrid_pgm_lipp_async,
                                  uint64_t);
