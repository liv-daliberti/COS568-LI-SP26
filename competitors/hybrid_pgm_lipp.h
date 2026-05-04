#ifndef TLI_HYBRID_PGM_LIPP_H
#define TLI_HYBRID_PGM_LIPP_H

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "../util.h"
#include "PGM-index/include/pgm_index.hpp"
#include "PGM-index/include/pgm_index_dynamic.hpp"
#include "base.h"
#include "lipp.h"
#include "lipp/src/core/lipp.h"

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPP : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPP(const std::vector<int>& params)
      : flush_threshold_keys_(params.empty()
                                  ? kDefaultFlushThreshold
                                  : static_cast<size_t>(std::max(params.front(), 1))) {}

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      lipp_ = std::make_unique<CoreLIPP>();
      pgm_buffer_ = std::make_unique<BufferIndex>();
      buffered_keys_ = 0;
      lipp_->bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    if (pgm_buffer_) {
      auto it = pgm_buffer_->find(lookup_key);
      if (it != pgm_buffer_->end()) {
        return it->value();
      }
    }

    if (lipp_) {
      uint64_t value = 0;
      if (lipp_->find(lookup_key, value)) {
        return value;
      }
    }

    return util::OVERFLOW;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;

    uint64_t result = 0;

    if (pgm_buffer_) {
      auto it = pgm_buffer_->lower_bound(lower_key);
      while (it != pgm_buffer_->end() && it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    if (lipp_) {
      auto it = lipp_->lower_bound(lower_key);
      while (it != lipp_->end() && it->comp.data.key <= upper_key) {
        result += it->comp.data.value;
        ++it;
      }
    }

    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    ensure_initialized();
    pgm_buffer_->insert(data.key, data.value);
    ++buffered_keys_;
    if (buffered_keys_ >= flush_threshold_keys_) {
      FlushBuffer();
    }
  }

  std::string name() const { return "HybridPGMLIPP"; }

  std::size_t size() const {
    std::size_t total = 0;
    if (lipp_) {
      total += lipp_->index_size();
    }
    if (pgm_buffer_) {
      total += pgm_buffer_->size_in_bytes();
    }
    return total;
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            std::to_string(flush_threshold_keys_)};
  }

 private:
  using BufferIndex = DynamicPGMIndex<
      KeyType, uint64_t, SearchClass,
      PGMIndex<KeyType, SearchClass, pgm_error, 16>>;
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultFlushThreshold = 100000;

  void ensure_initialized() {
    if (!lipp_) {
      lipp_ = std::make_unique<CoreLIPP>();
    }
    if (!pgm_buffer_) {
      pgm_buffer_ = std::make_unique<BufferIndex>();
    }
  }

  void FlushBuffer() {
    if (!lipp_ || !pgm_buffer_ || buffered_keys_ == 0) {
      return;
    }

    auto it = pgm_buffer_->lower_bound(std::numeric_limits<KeyType>::lowest());
    while (it != pgm_buffer_->end()) {
      lipp_->insert(it->key(), it->value());
      ++it;
    }

    pgm_buffer_ = std::make_unique<BufferIndex>();
    buffered_keys_ = 0;
  }

  size_t flush_threshold_keys_;
  size_t buffered_keys_ = 0;
  std::unique_ptr<CoreLIPP> lipp_;
  std::unique_ptr<BufferIndex> pgm_buffer_;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPIncremental : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPIncremental(const std::vector<int>& params)
      : flush_threshold_keys_(params.empty()
                                  ? kDefaultFlushThreshold
                                  : static_cast<size_t>(std::max(params.front(), 1))),
        drain_budget_(params.size() > 1
                          ? static_cast<size_t>(std::max(params[1], 1))
                          : kDefaultDrainBudget) {}

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      lipp_ = std::make_unique<CoreLIPP>();
      active_buffer_ = std::make_unique<BufferIndex>();
      draining_buffer_.reset();
      active_buffered_keys_ = 0;
      draining_buffered_keys_ = 0;
      drain_frontier_valid_ = false;
      lipp_->bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;

    if (active_buffer_) {
      auto it = active_buffer_->find(lookup_key);
      if (it != active_buffer_->end()) {
        return it->value();
      }
    }

    if (ShouldSearchDrainingBuffer(lookup_key)) {
      auto it = draining_buffer_->find(lookup_key);
      if (it != draining_buffer_->end()) {
        return it->value();
      }
    }

    if (lipp_) {
      uint64_t value = 0;
      if (lipp_->find(lookup_key, value)) {
        return value;
      }
    }

    return util::OVERFLOW;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;

    uint64_t result = 0;

    if (active_buffer_) {
      auto it = active_buffer_->lower_bound(lower_key);
      while (it != active_buffer_->end() && it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    if (draining_buffer_) {
      const KeyType* frontier = PendingDrainFrontier();
      const KeyType drain_start =
          frontier == nullptr || lower_key >= *frontier ? lower_key : *frontier;
      auto it = draining_buffer_->lower_bound(drain_start);
      while (it != draining_buffer_->end() && it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    if (lipp_) {
      auto it = lipp_->lower_bound(lower_key);
      while (it != lipp_->end() && it->comp.data.key <= upper_key) {
        result += it->comp.data.value;
        ++it;
      }
    }

    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    ensure_initialized();
    DrainSome(drain_budget_);
    active_buffer_->insert(data.key, data.value);
    ++active_buffered_keys_;
    RotateBuffersIfNeeded();
  }

  std::string name() const { return "HybridPGMLIPPIncremental"; }

  std::size_t size() const {
    std::size_t total = 0;
    if (lipp_) {
      total += lipp_->index_size();
    }
    if (active_buffer_) {
      total += active_buffer_->size_in_bytes();
    }
    if (draining_buffer_) {
      total += draining_buffer_->size_in_bytes();
    }
    return total;
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            std::to_string(flush_threshold_keys_) + "+" +
                std::to_string(drain_budget_)};
  }

 private:
  using BufferIndex = DynamicPGMIndex<
      KeyType, uint64_t, SearchClass,
      PGMIndex<KeyType, SearchClass, pgm_error, 16>>;
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultFlushThreshold = 100000;
  static constexpr size_t kDefaultDrainBudget = 256;

  void ensure_initialized() {
    if (!lipp_) {
      lipp_ = std::make_unique<CoreLIPP>();
    }
    if (!active_buffer_) {
      active_buffer_ = std::make_unique<BufferIndex>();
    }
  }

  const KeyType* PendingDrainFrontier() const {
    if (!draining_buffer_) {
      return nullptr;
    }

    if (!drain_frontier_valid_) {
      return nullptr;
    }

    return &drain_frontier_key_;
  }

  bool ShouldSearchDrainingBuffer(const KeyType& lookup_key) const {
    const KeyType* frontier = PendingDrainFrontier();
    return frontier != nullptr && lookup_key >= *frontier;
  }

  void RotateBuffersIfNeeded() const {
    if (active_buffered_keys_ < flush_threshold_keys_ || draining_buffer_) {
      return;
    }

    draining_buffer_ = std::move(active_buffer_);
    draining_buffered_keys_ = active_buffered_keys_;
    active_buffer_ = std::make_unique<BufferIndex>();
    active_buffered_keys_ = 0;
    auto first_pending =
        draining_buffer_->lower_bound(std::numeric_limits<KeyType>::lowest());
    if (first_pending == draining_buffer_->end()) {
      draining_buffer_.reset();
      draining_buffered_keys_ = 0;
      drain_frontier_valid_ = false;
      return;
    }

    drain_frontier_key_ = first_pending->key();
    drain_frontier_valid_ = true;
  }

  void DrainSome(size_t budget) const {
    if (!lipp_ || !draining_buffer_ || !drain_frontier_valid_ || budget == 0) {
      return;
    }

    auto drain_it = draining_buffer_->lower_bound(drain_frontier_key_);
    const auto draining_end = draining_buffer_->end();
    while (budget-- > 0 && drain_it != draining_end) {
      lipp_->insert(drain_it->key(), drain_it->value());
      ++drain_it;
      if (draining_buffered_keys_ > 0) {
        --draining_buffered_keys_;
      }
    }

    if (drain_it == draining_end) {
      draining_buffer_.reset();
      draining_buffered_keys_ = 0;
      drain_frontier_valid_ = false;
      RotateBuffersIfNeeded();
      return;
    }

    drain_frontier_key_ = drain_it->key();
  }

  size_t flush_threshold_keys_;
  size_t drain_budget_;
  mutable size_t active_buffered_keys_ = 0;
  mutable size_t draining_buffered_keys_ = 0;
  mutable KeyType drain_frontier_key_{};
  mutable bool drain_frontier_valid_ = false;
  mutable std::unique_ptr<CoreLIPP> lipp_;
  mutable std::unique_ptr<BufferIndex> active_buffer_;
  mutable std::unique_ptr<BufferIndex> draining_buffer_;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPAsyncExactLIPP : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPAsyncExactLIPP(const std::vector<int>& params) {
    (void) params;
  }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    return static_cast<size_t>(
        lipp_.find_value_or_fast(lookup_key,
                                 static_cast<uint64_t>(util::NOT_FOUND)));
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;
    auto it = lipp_.lower_bound(lower_key);
    while (it != lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    lipp_.insert(data.key, data.value);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const { return lipp_.index_size(); }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error), "1x"};
  }

 private:
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  CoreLIPP lipp_;
};

template <class KeyType, class SearchClass>
class HybridPGMLIPPAsyncStockLIPP : public Lipp<KeyType> {
 public:
  explicit HybridPGMLIPPAsyncStockLIPP(const std::vector<int>& params)
      : Lipp<KeyType>(params) {}

  std::string name() const { return "HybridPGMLIPPAsync"; }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), "stock"};
  }
};

template <class KeyType, class SearchClass, size_t pgm_error, int lookup_mode>
class HybridPGMLIPPAsyncExactLIPPVariant
    : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPAsyncExactLIPPVariant(const std::vector<int>& params) {
    (void) params;
  }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      ObserveBuildRange(data);
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
    });
  }

  inline __attribute__((always_inline)) size_t EqualityLookup(
      const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    if constexpr (lookup_mode == 4) {
      if (range_valid_ &&
          (lookup_key < min_key_ || lookup_key > max_key_)) {
        return util::NOT_FOUND;
      }
      return static_cast<size_t>(
          lipp_.find_value_or_fast(lookup_key,
                                   static_cast<uint64_t>(util::NOT_FOUND)));
    } else if constexpr (lookup_mode == 1) {
      uint64_t value = 0;
      if (lipp_.find(lookup_key, value)) {
        return value;
      }
      return util::NOT_FOUND;
    } else if constexpr (lookup_mode == 2) {
      return static_cast<size_t>(lipp_.template find_value_or_fast_hint<true>(
          lookup_key, static_cast<uint64_t>(util::NOT_FOUND)));
    } else if constexpr (lookup_mode == 3) {
      return static_cast<size_t>(lipp_.template find_value_or_fast_hint<false>(
          lookup_key, static_cast<uint64_t>(util::NOT_FOUND)));
    } else {
      return static_cast<size_t>(
          lipp_.find_value_or_fast(lookup_key,
                                   static_cast<uint64_t>(util::NOT_FOUND)));
    }
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;
    auto it = lipp_.lower_bound(lower_key);
    while (it != lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  inline __attribute__((always_inline)) void Insert(const KeyValue<KeyType>& data,
                                                    uint32_t thread_id) {
    (void) thread_id;
    if constexpr (lookup_mode == 4) {
      ObserveInsertedKey(data.key);
    }
    lipp_.insert(data.key, data.value);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const { return lipp_.index_size(); }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error), LookupPolicy()};
  }

 private:
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static std::string LookupPolicy() {
    if constexpr (lookup_mode == 1) {
      return "1xf";
    } else if constexpr (lookup_mode == 2) {
      return "1xcl";
    } else if constexpr (lookup_mode == 3) {
      return "1xcu";
    } else if constexpr (lookup_mode == 4) {
      return "1xrg";
    } else {
      return "1x";
    }
  }

  void ObserveBuildRange(const std::vector<KeyValue<KeyType>>& data) {
    if constexpr (lookup_mode == 4) {
      if (data.empty()) {
        range_valid_ = false;
        return;
      }
      min_key_ = data.front().key;
      max_key_ = data.front().key;
      for (const auto& item : data) {
        if (item.key < min_key_) {
          min_key_ = item.key;
        }
        if (item.key > max_key_) {
          max_key_ = item.key;
        }
      }
      range_valid_ = true;
    }
  }

  inline __attribute__((always_inline)) void ObserveInsertedKey(
      const KeyType& key) {
    if (!range_valid_) {
      min_key_ = key;
      max_key_ = key;
      range_valid_ = true;
      return;
    }
    if (key < min_key_) {
      min_key_ = key;
    }
    if (key > max_key_) {
      max_key_ = key;
    }
  }

  CoreLIPP lipp_;
  KeyType min_key_{};
  KeyType max_key_{};
  bool range_valid_ = false;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPSlackLIPP : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPSlackLIPP(const std::vector<int>& params)
      : slack_ppm_(params.empty() ? kDefaultSlackPpm
                                  : std::max(params.front(), 0)),
        rebuild_size_factor_(params.size() > 1 ? std::max(params[1], 1)
                                               : kDefaultRebuildSizeFactor),
        rebuild_collision_inv_(params.size() > 2 ? std::max(params[2], 1)
                                                 : kDefaultRebuildCollisionInv),
        slack_(static_cast<double>(slack_ppm_) / kSlackScale),
        lipp_(slack_, true, rebuild_size_factor_, rebuild_collision_inv_) {}

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    return static_cast<size_t>(
        lipp_.find_value_or_fast(lookup_key,
                                 static_cast<uint64_t>(util::NOT_FOUND)));
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;
    auto it = lipp_.lower_bound(lower_key);
    while (it != lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    lipp_.insert(data.key, data.value);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const { return lipp_.index_size(); }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    std::string policy =
        slack_ppm_ == 0 ? "1x" : "1xs" + std::to_string(slack_ppm_);
    if (rebuild_size_factor_ != kDefaultRebuildSizeFactor ||
        rebuild_collision_inv_ != kDefaultRebuildCollisionInv) {
      policy += "r" + std::to_string(rebuild_size_factor_) + "c" +
                std::to_string(rebuild_collision_inv_);
    }
    return {SearchClass::name(), std::to_string(pgm_error),
            policy};
  }

 private:
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr int kDefaultSlackPpm = 5000;
  static constexpr int kDefaultRebuildSizeFactor = 4;
  static constexpr int kDefaultRebuildCollisionInv = 10;
  static constexpr double kSlackScale = 1000000.0;

  int slack_ppm_;
  int rebuild_size_factor_;
  int rebuild_collision_inv_;
  double slack_;
  CoreLIPP lipp_;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPPartitionedLippDelta : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPPartitionedLippDelta(const std::vector<int>& params)
      : partitioned_shards_(params.empty()
                                ? kDefaultPartitionedShards
                                : static_cast<size_t>(std::max(params.front(), 1))) {}

  ~HybridPGMLIPPPartitionedLippDelta() { FinishPendingFlushes(); }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      FinishPendingFlushes();
      base_lipp_.bulk_load(loading_data.data(),
                           static_cast<int>(loading_data.size()));
      ObserveRoutingRange(data);
      InitializeDeltaLipps();
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    uint64_t value = 0;
    value = base_lipp_.find_value_or_fast(
        lookup_key, static_cast<uint64_t>(util::NOT_FOUND));
    if (value != static_cast<uint64_t>(util::NOT_FOUND)) {
      return value;
    }
    if (delta_key_count_ == 0) {
      return util::NOT_FOUND;
    }
    if (LookupDeltaLipp(lookup_key, value)) {
      return value;
    }
    return util::NOT_FOUND;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;

    for (size_t shard = 0; shard < delta_lipps_.size(); ++shard) {
      if (delta_counts_[shard] == 0 ||
          !RangeIntersectsDelta(shard, lower_key, upper_key)) {
        continue;
      }
      auto it = delta_lipps_[shard]->lower_bound(lower_key);
      while (it != delta_lipps_[shard]->end() &&
             it->comp.data.key <= upper_key) {
        result += it->comp.data.value;
        ++it;
      }
    }

    auto it = base_lipp_.lower_bound(lower_key);
    while (it != base_lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    if (delta_lipps_.empty()) {
      InitializeDeltaLipps();
    }
    const size_t shard = PartitionForKey(data.key);
    delta_lipps_[shard]->insert(data.key, data.value);
    ++delta_counts_[shard];
    ++delta_key_count_;
    ObserveDeltaKey(shard, data.key);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const {
    FinishPendingFlushes();
    return base_lipp_.index_size();
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            "1rl" + std::to_string(partitioned_shards_) + "p"};
  }

 private:
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultPartitionedShards = 32;

  static bool AuditEnabled() {
    static const bool enabled = std::getenv("HYBRID_PGM_LIPP_AUDIT") != nullptr;
    return enabled;
  }

  static size_t CountLippEntries(const std::unique_ptr<CoreLIPP>& lipp) {
    if (!lipp) {
      return 0;
    }
    size_t count = 0;
    auto it = lipp->lower_bound(std::numeric_limits<KeyType>::lowest());
    while (it != lipp->end()) {
      ++count;
      ++it;
    }
    return count;
  }

  void ObserveRoutingRange(const std::vector<KeyValue<KeyType>>& data) const {
    if (data.empty()) {
      routing_range_valid_ = false;
      return;
    }
    routing_min_key_ = data.front().key;
    routing_max_key_ = data.front().key;
    for (const auto& item : data) {
      if (item.key < routing_min_key_) {
        routing_min_key_ = item.key;
      }
      if (item.key > routing_max_key_) {
        routing_max_key_ = item.key;
      }
    }
    routing_shift_ = ComputeRoutingShift(
        UnsignedDistance(routing_min_key_, routing_max_key_),
        partitioned_shards_);
    routing_range_valid_ = true;
  }

  void InitializeDeltaLipps() const {
    delta_lipps_.clear();
    delta_lipps_.reserve(partitioned_shards_);
    for (size_t i = 0; i < partitioned_shards_; ++i) {
      delta_lipps_.push_back(std::make_unique<CoreLIPP>());
    }
    delta_counts_.assign(partitioned_shards_, 0);
    delta_min_keys_.assign(partitioned_shards_, KeyType{});
    delta_max_keys_.assign(partitioned_shards_, KeyType{});
    delta_range_valid_.assign(partitioned_shards_, false);
    delta_key_count_ = 0;
  }

  size_t PartitionForKey(const KeyType& key) const {
    if (partitioned_shards_ <= 1 || !routing_range_valid_) {
      return 0;
    }
    if (key <= routing_min_key_) {
      return 0;
    }
    if (key >= routing_max_key_) {
      return partitioned_shards_ - 1;
    }

    const auto offset =
        static_cast<UnsignedKey>(key) - static_cast<UnsignedKey>(routing_min_key_);
    const auto shard = static_cast<size_t>(offset >> routing_shift_);
    return shard >= partitioned_shards_ ? partitioned_shards_ - 1 : shard;
  }

  using UnsignedKey =
      typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;

  static UnsignedKey UnsignedDistance(const KeyType& lo, const KeyType& hi) {
    const auto ulo = static_cast<UnsignedKey>(lo);
    const auto uhi = static_cast<UnsignedKey>(hi);
    return uhi >= ulo ? static_cast<UnsignedKey>(uhi - ulo)
                      : static_cast<UnsignedKey>(ulo - uhi);
  }

  static uint8_t ComputeRoutingShift(UnsignedKey span, size_t shard_count) {
    uint8_t shift = 0;
    while (shift < std::numeric_limits<UnsignedKey>::digits &&
           (span >> shift) >= shard_count) {
      ++shift;
    }
    return shift;
  }

  void ObserveDeltaKey(size_t shard, const KeyType& key) const {
    if (!delta_range_valid_[shard]) {
      delta_min_keys_[shard] = key;
      delta_max_keys_[shard] = key;
      delta_range_valid_[shard] = true;
      return;
    }
    if (key < delta_min_keys_[shard]) {
      delta_min_keys_[shard] = key;
    }
    if (key > delta_max_keys_[shard]) {
      delta_max_keys_[shard] = key;
    }
  }

  bool DeltaMayContain(size_t shard, const KeyType& key) const {
    return delta_range_valid_[shard] && key >= delta_min_keys_[shard] &&
           key <= delta_max_keys_[shard];
  }

  bool RangeIntersectsDelta(size_t shard, const KeyType& lower_key,
                            const KeyType& upper_key) const {
    return delta_range_valid_[shard] && upper_key >= delta_min_keys_[shard] &&
           lower_key <= delta_max_keys_[shard];
  }

  bool LookupDeltaLipp(const KeyType& lookup_key, uint64_t& value) const {
    const size_t shard = PartitionForKey(lookup_key);
    if (shard >= delta_lipps_.size() || delta_counts_[shard] == 0 ||
        !DeltaMayContain(shard, lookup_key)) {
      return false;
    }
    value = delta_lipps_[shard]->find_value_or_fast(
        lookup_key, static_cast<uint64_t>(util::NOT_FOUND));
    return value != static_cast<uint64_t>(util::NOT_FOUND);
  }

  void AuditDeltaCount(size_t shard) const {
    if (!AuditEnabled()) {
      return;
    }
    const size_t actual = CountLippEntries(delta_lipps_[shard]);
    if (actual != delta_counts_[shard]) {
      util::fail("HybridPGMLIPPAsync LIPP-delta audit failed for shard " +
                 std::to_string(shard) + ": expected " +
                 std::to_string(delta_counts_[shard]) +
                 " keys, saw " + std::to_string(actual));
    }
  }

  void FinishPendingFlushes() const {
    if (delta_lipps_.empty() || delta_key_count_ == 0) {
      return;
    }

    size_t total_drained = 0;
    for (size_t shard = 0; shard < delta_lipps_.size(); ++shard) {
      if (delta_counts_[shard] == 0) {
        AuditDeltaCount(shard);
        continue;
      }
      AuditDeltaCount(shard);
      size_t shard_drained = 0;
      auto it =
          delta_lipps_[shard]->lower_bound(std::numeric_limits<KeyType>::lowest());
      while (it != delta_lipps_[shard]->end()) {
        base_lipp_.insert(it->comp.data.key, it->comp.data.value);
        ++shard_drained;
        ++total_drained;
        ++it;
      }
      if (AuditEnabled() && shard_drained != delta_counts_[shard]) {
        util::fail("HybridPGMLIPPAsync LIPP-delta drain audit failed for shard " +
                   std::to_string(shard) + ": expected " +
                   std::to_string(delta_counts_[shard]) +
                   " keys, saw " + std::to_string(shard_drained));
      }
    }
    if (AuditEnabled() && total_drained != delta_key_count_) {
      util::fail("HybridPGMLIPPAsync LIPP-delta total audit failed: expected " +
                 std::to_string(delta_key_count_) + " keys, saw " +
                 std::to_string(total_drained));
    }
    InitializeDeltaLipps();
  }

  size_t partitioned_shards_;
  mutable CoreLIPP base_lipp_;
  mutable std::vector<std::unique_ptr<CoreLIPP>> delta_lipps_;
  mutable std::vector<size_t> delta_counts_;
  mutable std::vector<KeyType> delta_min_keys_;
  mutable std::vector<KeyType> delta_max_keys_;
  mutable std::vector<bool> delta_range_valid_;
  mutable size_t delta_key_count_ = 0;
  mutable KeyType routing_min_key_{};
  mutable KeyType routing_max_key_{};
  mutable uint8_t routing_shift_ = 0;
  mutable bool routing_range_valid_ = false;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPShardedLIPP : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPShardedLIPP(const std::vector<int>& params)
      : shard_count_(params.empty()
                         ? kDefaultShardCount
                         : static_cast<size_t>(std::max(params.front(), 1))) {}

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;

    return util::timing([&] {
      ObserveRoutingRange(data);
      std::vector<std::pair<KeyType, uint64_t>> loading_data;
      loading_data.reserve(data.size());
      for (const auto& item : data) {
        loading_data.emplace_back(item.key, item.value);
      }

      shards_.clear();
      shards_.reserve(shard_count_);
      shard_counts_.assign(shard_count_, 0);
      size_t begin = 0;
      for (size_t shard = 0; shard < shard_count_; ++shard) {
        size_t end = begin;
        while (end < data.size() && ShardForKey(data[end].key) == shard) {
          ++end;
        }
        shards_.push_back(std::make_unique<CoreLIPP>());
        shards_[shard]->bulk_load(loading_data.data() + begin,
                                  static_cast<int>(end - begin));
        shard_counts_[shard] = end - begin;
        begin = end;
      }
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    if (shards_.empty()) {
      return util::NOT_FOUND;
    }
    const size_t shard = ShardForKey(lookup_key);
    if (shard_counts_[shard] == 0) {
      return util::NOT_FOUND;
    }
    return static_cast<size_t>(shards_[shard]->find_value_or_fast(
        lookup_key, static_cast<uint64_t>(util::NOT_FOUND)));
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    if (shards_.empty()) {
      return 0;
    }
    uint64_t result = 0;
    const size_t first_shard = ShardForKey(lower_key);
    const size_t last_shard = ShardForKey(upper_key);
    for (size_t shard = first_shard; shard <= last_shard && shard < shards_.size();
         ++shard) {
      if (shard_counts_[shard] == 0) {
        continue;
      }
      auto it = shards_[shard]->lower_bound(lower_key);
      while (it != shards_[shard]->end() && it->comp.data.key <= upper_key) {
        result += it->comp.data.value;
        ++it;
      }
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    if (shards_.empty()) {
      InitializeEmptyShards();
    }
    const size_t shard = ShardForKey(data.key);
    shards_[shard]->insert(data.key, data.value);
    ++shard_counts_[shard];
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const {
    std::size_t total = 0;
    for (const auto& shard : shards_) {
      if (shard) {
        total += shard->index_size();
      }
    }
    return total;
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            "1s" + std::to_string(shard_count_)};
  }

 private:
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultShardCount = 64;

  void ObserveRoutingRange(const std::vector<KeyValue<KeyType>>& data) const {
    if (data.empty()) {
      routing_min_key_ = KeyType{};
      routing_max_key_ = KeyType{};
      routing_range_valid_ = false;
      return;
    }
    routing_min_key_ = data.front().key;
    routing_max_key_ = data.front().key;
    for (const auto& item : data) {
      if (item.key < routing_min_key_) {
        routing_min_key_ = item.key;
      }
      if (item.key > routing_max_key_) {
        routing_max_key_ = item.key;
      }
    }
    routing_shift_ = ComputeRoutingShift(
        UnsignedDistance(routing_min_key_, routing_max_key_), shard_count_);
    routing_range_valid_ = true;
  }

  void InitializeEmptyShards() const {
    shards_.clear();
    shards_.reserve(shard_count_);
    for (size_t shard = 0; shard < shard_count_; ++shard) {
      shards_.push_back(std::make_unique<CoreLIPP>());
    }
    shard_counts_.assign(shard_count_, 0);
  }

  size_t ShardForKey(const KeyType& key) const {
    if (shard_count_ <= 1 || !routing_range_valid_) {
      return 0;
    }
    if (key <= routing_min_key_) {
      return 0;
    }
    if (key >= routing_max_key_) {
      return shard_count_ - 1;
    }

    const auto offset =
        static_cast<UnsignedKey>(key) - static_cast<UnsignedKey>(routing_min_key_);
    const auto shard = static_cast<size_t>(offset >> routing_shift_);
    return shard >= shard_count_ ? shard_count_ - 1 : shard;
  }

  using UnsignedKey =
      typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;

  static UnsignedKey UnsignedDistance(const KeyType& lo, const KeyType& hi) {
    const auto ulo = static_cast<UnsignedKey>(lo);
    const auto uhi = static_cast<UnsignedKey>(hi);
    return uhi >= ulo ? static_cast<UnsignedKey>(uhi - ulo)
                      : static_cast<UnsignedKey>(ulo - uhi);
  }

  static uint8_t ComputeRoutingShift(UnsignedKey span, size_t shard_count) {
    uint8_t shift = 0;
    while (shift < std::numeric_limits<UnsignedKey>::digits &&
           (span >> shift) >= shard_count) {
      ++shift;
    }
    return shift;
  }

  size_t shard_count_;
  mutable std::vector<std::unique_ptr<CoreLIPP>> shards_;
  mutable std::vector<size_t> shard_counts_;
  mutable KeyType routing_min_key_{};
  mutable KeyType routing_max_key_{};
  mutable uint8_t routing_shift_ = 0;
  mutable bool routing_range_valid_ = false;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPGuardedDelta : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPGuardedDelta(const std::vector<int>& params)
      : guard_bucket_count_(params.empty()
                                ? kDefaultGuardBucketCount
                                : static_cast<size_t>(std::max(params.front(), 1))),
        delta_shard_count_(params.size() < 2
                               ? kDefaultDeltaShardCount
                               : static_cast<size_t>(std::max(params[1], 1))) {}

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
      delta_key_count_ = 0;
      ObserveRoutingRange(data);
      delta_shards_.clear();
      delta_shards_.resize(delta_shard_count_);
      delta_shard_counts_.assign(delta_shard_count_, 0);
      guard_counts_.assign(guard_bucket_count_, 0);
      guard_min_keys_.assign(guard_bucket_count_, KeyType{});
      guard_max_keys_.assign(guard_bucket_count_, KeyType{});
      guard_valid_.assign(guard_bucket_count_, false);
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    uint64_t value = 0;
    value = lipp_.find_value_or_fast(
        lookup_key, static_cast<uint64_t>(util::NOT_FOUND));
    if (value != static_cast<uint64_t>(util::NOT_FOUND)) {
      return value;
    }

    if (delta_key_count_ == 0 || !GuardMayContain(lookup_key)) {
      return util::NOT_FOUND;
    }

    const size_t shard = DeltaShardForKey(lookup_key);
    if (shard >= delta_shards_.size() || !delta_shards_[shard]) {
      return util::NOT_FOUND;
    }
    auto it = delta_shards_[shard]->find(lookup_key);
    if (it == delta_shards_[shard]->end()) {
      return util::NOT_FOUND;
    }
    return it->value();
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;
    auto lipp_it = lipp_.lower_bound(lower_key);
    while (lipp_it != lipp_.end() && lipp_it->comp.data.key <= upper_key) {
      result += lipp_it->comp.data.value;
      ++lipp_it;
    }

    if (delta_key_count_ == 0) {
      return result;
    }
    for (const auto& shard : delta_shards_) {
      if (!shard) {
        continue;
      }
      auto delta_it = shard->lower_bound(lower_key);
      while (delta_it != shard->end() && delta_it->key() <= upper_key) {
        result += delta_it->value();
        ++delta_it;
      }
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    const size_t shard = DeltaShardForKey(data.key);
    if (!delta_shards_[shard]) {
      delta_shards_[shard] = std::make_unique<BufferIndex>();
    }
    delta_shards_[shard]->insert(data.key, data.value);
    ++delta_shard_counts_[shard];
    ++delta_key_count_;
    ObserveGuardKey(data.key);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const {
    AuditGuardCounts();
    std::size_t total = lipp_.index_size() +
           delta_shards_.size() * sizeof(std::unique_ptr<BufferIndex>) +
           delta_shard_counts_.size() * sizeof(uint32_t) +
           guard_counts_.size() * sizeof(uint32_t) +
           guard_min_keys_.size() * sizeof(KeyType) +
           guard_max_keys_.size() * sizeof(KeyType) +
           guard_valid_.size() * sizeof(bool);
    for (const auto& shard : delta_shards_) {
      if (shard) {
        total += shard->size_in_bytes();
      }
    }
    return total;
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            "1g" + std::to_string(guard_bucket_count_) + "s" +
                std::to_string(delta_shard_count_)};
  }

 private:
  using BufferIndex = DynamicPGMIndex<
      KeyType, uint64_t, SearchClass,
      PGMIndex<KeyType, SearchClass, pgm_error, 16>>;
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultGuardBucketCount = 1u << 20;
  static constexpr size_t kDefaultDeltaShardCount = 4096;

  void ObserveRoutingRange(const std::vector<KeyValue<KeyType>>& data) const {
    if (data.empty()) {
      routing_min_key_ = KeyType{};
      routing_max_key_ = KeyType{};
      routing_guard_shift_ = 0;
      routing_delta_shift_ = 0;
      routing_range_valid_ = false;
      return;
    }

    routing_min_key_ = data.front().key;
    routing_max_key_ = data.front().key;
    for (const auto& item : data) {
      if (item.key < routing_min_key_) {
        routing_min_key_ = item.key;
      }
      if (item.key > routing_max_key_) {
        routing_max_key_ = item.key;
      }
    }

    const auto span = UnsignedDistance(routing_min_key_, routing_max_key_);
    routing_guard_shift_ = ComputeRoutingShift(span, guard_bucket_count_);
    routing_delta_shift_ = ComputeRoutingShift(span, delta_shard_count_);
    routing_range_valid_ = true;
  }

  using UnsignedKey =
      typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;

  static UnsignedKey UnsignedDistance(const KeyType& lo, const KeyType& hi) {
    const auto ulo = static_cast<UnsignedKey>(lo);
    const auto uhi = static_cast<UnsignedKey>(hi);
    return uhi >= ulo ? static_cast<UnsignedKey>(uhi - ulo)
                      : static_cast<UnsignedKey>(ulo - uhi);
  }

  static uint8_t ComputeRoutingShift(UnsignedKey span, size_t bucket_count) {
    uint8_t shift = 0;
    while (shift < std::numeric_limits<UnsignedKey>::digits &&
           (span >> shift) >= bucket_count) {
      ++shift;
    }
    return shift;
  }

  size_t RoutedBucketForKey(const KeyType& key, size_t bucket_count,
                            uint8_t shift) const {
    if (bucket_count <= 1 || !routing_range_valid_) {
      return 0;
    }
    if (key <= routing_min_key_) {
      return 0;
    }
    if (key >= routing_max_key_) {
      return bucket_count - 1;
    }

    const auto offset =
        static_cast<UnsignedKey>(key) - static_cast<UnsignedKey>(routing_min_key_);
    size_t bucket = static_cast<size_t>(offset >> shift);
    return bucket >= bucket_count ? bucket_count - 1 : bucket;
  }

  size_t GuardBucketForKey(const KeyType& key) const {
    return RoutedBucketForKey(key, guard_bucket_count_, routing_guard_shift_);
  }

  size_t DeltaShardForKey(const KeyType& key) const {
    return RoutedBucketForKey(key, delta_shard_count_, routing_delta_shift_);
  }

  void ObserveGuardKey(const KeyType& key) const {
    const size_t bucket = GuardBucketForKey(key);
    if (!guard_valid_[bucket]) {
      guard_min_keys_[bucket] = key;
      guard_max_keys_[bucket] = key;
      guard_valid_[bucket] = true;
    } else {
      if (key < guard_min_keys_[bucket]) {
        guard_min_keys_[bucket] = key;
      }
      if (key > guard_max_keys_[bucket]) {
        guard_max_keys_[bucket] = key;
      }
    }
    ++guard_counts_[bucket];
  }

  bool GuardMayContain(const KeyType& key) const {
    const size_t bucket = GuardBucketForKey(key);
    return bucket < guard_counts_.size() && guard_counts_[bucket] > 0 &&
           guard_valid_[bucket] && key >= guard_min_keys_[bucket] &&
           key <= guard_max_keys_[bucket];
  }

  void AuditGuardCounts() const {
    if (!std::getenv("HYBRID_PGM_LIPP_AUDIT")) {
      return;
    }
    size_t counted = 0;
    for (uint32_t count : guard_counts_) {
      counted += count;
    }
    size_t delta_counted = 0;
    for (uint32_t count : delta_shard_counts_) {
      delta_counted += count;
    }
    if (counted != delta_key_count_) {
      util::fail("HybridPGMLIPPAsync guard audit failed: counted " +
                 std::to_string(counted) + " inserted keys, expected " +
                 std::to_string(delta_key_count_));
    }
    if (delta_counted != delta_key_count_) {
      util::fail("HybridPGMLIPPAsync guarded-delta shard audit failed: counted " +
                 std::to_string(delta_counted) + " inserted keys, expected " +
                 std::to_string(delta_key_count_));
    }
  }

  size_t guard_bucket_count_;
  size_t delta_shard_count_;
  mutable CoreLIPP lipp_;
  mutable std::vector<std::unique_ptr<BufferIndex>> delta_shards_;
  mutable std::vector<uint32_t> delta_shard_counts_;
  mutable size_t delta_key_count_ = 0;
  mutable std::vector<uint32_t> guard_counts_;
  mutable std::vector<KeyType> guard_min_keys_;
  mutable std::vector<KeyType> guard_max_keys_;
  mutable std::vector<bool> guard_valid_;
  mutable KeyType routing_min_key_{};
  mutable KeyType routing_max_key_{};
  mutable uint8_t routing_guard_shift_ = 0;
  mutable uint8_t routing_delta_shift_ = 0;
  mutable bool routing_range_valid_ = false;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPPartitionedDelta : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPPartitionedDelta(const std::vector<int>& params)
      : partitioned_shards_(params.empty()
                                ? kDefaultPartitionedShards
                                : static_cast<size_t>(std::max(params.front(), 1))) {}

  ~HybridPGMLIPPPartitionedDelta() { FinishPendingFlushes(); }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    (void) num_threads;
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      FinishPendingFlushes();
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
      ObserveRoutingRange(data);
      InitializePartitionedBuffers();
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;
    uint64_t value = 0;
    if (lipp_.find(lookup_key, value)) {
      return value;
    }
    if (partitioned_buffered_keys_ == 0) {
      return util::NOT_FOUND;
    }
    if (LookupPartitionedBuffer(lookup_key, value)) {
      return value;
    }
    return util::NOT_FOUND;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;
    uint64_t result = 0;
    for (size_t shard = 0; shard < partitioned_buffers_.size(); ++shard) {
      if (partitioned_counts_[shard] == 0 ||
          !RangeIntersectsPartition(shard, lower_key, upper_key)) {
        continue;
      }
      const KeyType scan_start =
          lower_key > partitioned_min_keys_[shard]
              ? lower_key
              : partitioned_min_keys_[shard];
      auto it = partitioned_buffers_[shard]->lower_bound(scan_start);
      while (it != partitioned_buffers_[shard]->end() &&
             it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    auto it = lipp_.lower_bound(lower_key);
    while (it != lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    if (partitioned_buffers_.empty()) {
      InitializePartitionedBuffers();
    }
    const size_t shard = PartitionForKey(data.key);
    if (!partitioned_buffers_[shard]) {
      partitioned_buffers_[shard] = std::make_unique<BufferIndex>();
    }
    partitioned_buffers_[shard]->insert(data.key, data.value);
    ++partitioned_counts_[shard];
    ++partitioned_buffered_keys_;
    ObservePartitionKey(shard, data.key);
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const {
    FinishPendingFlushes();
    return lipp_.index_size();
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    return {SearchClass::name(), std::to_string(pgm_error),
            "1r" + std::to_string(partitioned_shards_) + "p"};
  }

 private:
  using BufferIndex = DynamicPGMIndex<
      KeyType, uint64_t, SearchClass,
      PGMIndex<KeyType, SearchClass, pgm_error, 16>>;
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  static constexpr size_t kDefaultPartitionedShards = 32;

  static bool AuditEnabled() {
    static const bool enabled = std::getenv("HYBRID_PGM_LIPP_AUDIT") != nullptr;
    return enabled;
  }

  static size_t CountBufferEntries(const std::unique_ptr<BufferIndex>& buffer) {
    if (!buffer) {
      return 0;
    }
    size_t count = 0;
    auto it = buffer->lower_bound(std::numeric_limits<KeyType>::lowest());
    while (it != buffer->end()) {
      ++count;
      ++it;
    }
    return count;
  }

  void ObserveRoutingRange(const std::vector<KeyValue<KeyType>>& data) const {
    if (data.empty()) {
      routing_range_valid_ = false;
      return;
    }
    routing_min_key_ = data.front().key;
    routing_max_key_ = data.front().key;
    for (const auto& item : data) {
      if (item.key < routing_min_key_) {
        routing_min_key_ = item.key;
      }
      if (item.key > routing_max_key_) {
        routing_max_key_ = item.key;
      }
    }
    routing_shift_ = ComputeRoutingShift(
        UnsignedDistance(routing_min_key_, routing_max_key_),
        partitioned_shards_);
    routing_range_valid_ = true;
  }

  void InitializePartitionedBuffers() const {
    partitioned_buffers_.clear();
    partitioned_buffers_.resize(partitioned_shards_);
    partitioned_counts_.assign(partitioned_shards_, 0);
    partitioned_min_keys_.assign(partitioned_shards_, KeyType{});
    partitioned_max_keys_.assign(partitioned_shards_, KeyType{});
    partitioned_range_valid_.assign(partitioned_shards_, false);
    partitioned_buffered_keys_ = 0;
  }

  size_t PartitionForKey(const KeyType& key) const {
    if (partitioned_shards_ <= 1 || !routing_range_valid_) {
      return 0;
    }
    if (key <= routing_min_key_) {
      return 0;
    }
    if (key >= routing_max_key_) {
      return partitioned_shards_ - 1;
    }

    const auto offset =
        static_cast<UnsignedKey>(key) -
        static_cast<UnsignedKey>(routing_min_key_);
    const auto shard = static_cast<size_t>(offset >> routing_shift_);
    return shard >= partitioned_shards_ ? partitioned_shards_ - 1 : shard;
  }

  using UnsignedKey =
      typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;

  static UnsignedKey UnsignedDistance(const KeyType& lo, const KeyType& hi) {
    const auto ulo = static_cast<UnsignedKey>(lo);
    const auto uhi = static_cast<UnsignedKey>(hi);
    return uhi >= ulo ? static_cast<UnsignedKey>(uhi - ulo)
                      : static_cast<UnsignedKey>(ulo - uhi);
  }

  static uint8_t ComputeRoutingShift(UnsignedKey span, size_t shard_count) {
    uint8_t shift = 0;
    while (shift < std::numeric_limits<UnsignedKey>::digits &&
           (span >> shift) >= shard_count) {
      ++shift;
    }
    return shift;
  }

  void ObservePartitionKey(size_t shard, const KeyType& key) const {
    if (!partitioned_range_valid_[shard]) {
      partitioned_min_keys_[shard] = key;
      partitioned_max_keys_[shard] = key;
      partitioned_range_valid_[shard] = true;
      return;
    }
    if (key < partitioned_min_keys_[shard]) {
      partitioned_min_keys_[shard] = key;
    }
    if (key > partitioned_max_keys_[shard]) {
      partitioned_max_keys_[shard] = key;
    }
  }

  bool PartitionMayContain(size_t shard, const KeyType& lookup_key) const {
    return partitioned_range_valid_[shard] &&
           lookup_key >= partitioned_min_keys_[shard] &&
           lookup_key <= partitioned_max_keys_[shard];
  }

  bool RangeIntersectsPartition(size_t shard, const KeyType& lower_key,
                                const KeyType& upper_key) const {
    return partitioned_range_valid_[shard] &&
           upper_key >= partitioned_min_keys_[shard] &&
           lower_key <= partitioned_max_keys_[shard];
  }

  bool LookupPartitionedBuffer(const KeyType& lookup_key, uint64_t& value) const {
    if (partitioned_buffers_.empty()) {
      return false;
    }
    const size_t shard = PartitionForKey(lookup_key);
    if (shard >= partitioned_buffers_.size() || partitioned_counts_[shard] == 0 ||
        !partitioned_buffers_[shard] ||
        !PartitionMayContain(shard, lookup_key)) {
      return false;
    }
    auto it = partitioned_buffers_[shard]->find(lookup_key);
    if (it == partitioned_buffers_[shard]->end()) {
      return false;
    }
    value = it->value();
    return true;
  }

  void AuditPartitionedBufferCount(size_t shard) const {
    if (!AuditEnabled()) {
      return;
    }
    if (!partitioned_buffers_[shard]) {
      if (partitioned_counts_[shard] == 0) {
        return;
      }
      util::fail("HybridPGMLIPPAsync partition audit failed for shard " +
                 std::to_string(shard) + ": nonzero count without DPGM shard");
    }
    const size_t actual = CountBufferEntries(partitioned_buffers_[shard]);
    if (actual != partitioned_counts_[shard]) {
      util::fail("HybridPGMLIPPAsync partition audit failed for shard " +
                 std::to_string(shard) + ": expected " +
                 std::to_string(partitioned_counts_[shard]) +
                 " buffered keys, saw " + std::to_string(actual));
    }
  }

  void FinishPendingFlushes() const {
    if (partitioned_buffers_.empty() || partitioned_buffered_keys_ == 0) {
      return;
    }

    size_t total_drained = 0;
    for (size_t shard = 0; shard < partitioned_buffers_.size(); ++shard) {
      if (partitioned_counts_[shard] == 0) {
        AuditPartitionedBufferCount(shard);
        continue;
      }
      if (!partitioned_buffers_[shard]) {
        util::fail("HybridPGMLIPPAsync partition drain failed for shard " +
                   std::to_string(shard) + ": nonzero count without DPGM shard");
      }
      AuditPartitionedBufferCount(shard);
      size_t shard_drained = 0;
      auto it =
          partitioned_buffers_[shard]->lower_bound(std::numeric_limits<KeyType>::lowest());
      while (it != partitioned_buffers_[shard]->end()) {
        lipp_.insert(it->key(), it->value());
        ++shard_drained;
        ++total_drained;
        ++it;
      }
      if (AuditEnabled() && shard_drained != partitioned_counts_[shard]) {
        util::fail("HybridPGMLIPPAsync partition drain audit failed for shard " +
                   std::to_string(shard) + ": expected " +
                   std::to_string(partitioned_counts_[shard]) +
                   " drained keys, saw " + std::to_string(shard_drained));
      }
    }
    if (AuditEnabled() && total_drained != partitioned_buffered_keys_) {
      util::fail("HybridPGMLIPPAsync partition total audit failed: expected " +
                 std::to_string(partitioned_buffered_keys_) +
                 " drained keys, saw " + std::to_string(total_drained));
    }
    InitializePartitionedBuffers();
  }

  size_t partitioned_shards_;
  mutable CoreLIPP lipp_;
  mutable std::vector<std::unique_ptr<BufferIndex>> partitioned_buffers_;
  mutable std::vector<size_t> partitioned_counts_;
  mutable std::vector<KeyType> partitioned_min_keys_;
  mutable std::vector<KeyType> partitioned_max_keys_;
  mutable std::vector<bool> partitioned_range_valid_;
  mutable size_t partitioned_buffered_keys_ = 0;
  mutable KeyType routing_min_key_{};
  mutable KeyType routing_max_key_{};
  mutable uint8_t routing_shift_ = 0;
  mutable bool routing_range_valid_ = false;
};

template <class KeyType, class SearchClass, size_t pgm_error>
class HybridPGMLIPPAsync : public Competitor<KeyType, SearchClass> {
 public:
  explicit HybridPGMLIPPAsync(const std::vector<int>& params)
      : flush_threshold_keys_(params.empty()
                                  ? kDefaultFlushThreshold
                                  : static_cast<size_t>(std::max(params.front(), 1))),
        probe_mode_(ParseProbeMode(params)),
        partitioned_shards_(ParsePartitionedShardCount(params)),
        partitioned_read_mode_(probe_mode_ == ProbeMode::kReadOptimized &&
                               partitioned_shards_ > 1),
        immediate_lipp_mode_((probe_mode_ == ProbeMode::kExactLipp) ||
                             (probe_mode_ == ProbeMode::kReadOptimized &&
                              flush_threshold_keys_ == 1 &&
                              !partitioned_read_mode_)),
        deferred_write_mode_(probe_mode_ == ProbeMode::kWriteOptimized &&
                             flush_threshold_keys_ >=
                                 kDeferredWriteFlushThreshold) {}

  ~HybridPGMLIPPAsync() {
    if (!immediate_lipp_mode_) {
      FinishPendingFlushes();
    }
  }

  uint64_t Build(const std::vector<KeyValue<KeyType>>& data, size_t num_threads) {
    std::vector<std::pair<KeyType, uint64_t>> loading_data;
    loading_data.reserve(data.size());
    for (const auto& item : data) {
      loading_data.emplace_back(item.key, item.value);
    }

    return util::timing([&] {
      if (!immediate_lipp_mode_) {
        FinishPendingFlushes();
      }
      active_buffer_ =
          immediate_lipp_mode_ || partitioned_read_mode_
              ? nullptr
              : std::make_unique<BufferIndex>();
      flushing_buffer_.reset();
      active_buffered_keys_ = 0;
      flushing_buffered_keys_ = 0;
      active_range_valid_ = false;
      flushing_range_valid_ = false;
      partitioned_buffered_keys_ = 0;
      routing_range_valid_ = false;
      background_flush_done_.store(true, std::memory_order_release);
      lipp_.bulk_load(loading_data.data(), static_cast<int>(loading_data.size()));
      ObserveRoutingRange(data);
      if (partitioned_read_mode_) {
        InitializePartitionedBuffers();
      } else {
        ResetPartitionedBuffers();
      }
    });
  }

  size_t EqualityLookup(const KeyType& lookup_key, uint32_t thread_id) const {
    (void) thread_id;

    uint64_t value = 0;

    if (immediate_lipp_mode_) {
      if (lipp_.find(lookup_key, value)) {
        return value;
      }
      return util::OVERFLOW;
    }

    if (partitioned_read_mode_) {
      value = lipp_.find_value_or_fast(
          lookup_key, static_cast<uint64_t>(util::NOT_FOUND));
      if (value != static_cast<uint64_t>(util::NOT_FOUND)) {
        return value;
      }
      if (partitioned_buffered_keys_ == 0) {
        return util::OVERFLOW;
      }
      if (LookupPartitionedBuffer(lookup_key, value)) {
        return value;
      }
      return util::OVERFLOW;
    }

    if (PreferReadOptimizedProbeOrder()) {
      if (FindInLipp(lookup_key, value)) {
        return value;
      }
      if (!HasBufferedKeys()) {
        return util::OVERFLOW;
      }
      if (active_buffered_keys_ > 0 &&
          LookupBuffer(active_buffer_, active_range_valid_, active_min_key_,
                       active_max_key_, lookup_key, value)) {
        return value;
      }
      if (flushing_buffered_keys_ > 0 &&
          LookupBuffer(flushing_buffer_, flushing_range_valid_, flushing_min_key_,
                       flushing_max_key_, lookup_key, value)) {
        return value;
      }
      return util::OVERFLOW;
    }

    if (deferred_write_mode_) {
      value = lipp_.find_value_or_fast(
          lookup_key, static_cast<uint64_t>(util::NOT_FOUND));
      if (value != static_cast<uint64_t>(util::NOT_FOUND)) {
        return value;
      }
      if (active_buffered_keys_ > 0 &&
          LookupBufferNoRange(active_buffer_, lookup_key, value)) {
        return value;
      }
      return util::OVERFLOW;
    }

    if (!HasBufferedKeys()) {
      if (FindInLipp(lookup_key, value)) {
        return value;
      }
      return util::OVERFLOW;
    }

    if (active_buffered_keys_ > 0 &&
        LookupBuffer(active_buffer_, active_range_valid_, active_min_key_,
                     active_max_key_, lookup_key, value)) {
      return value;
    }

    if (flushing_buffered_keys_ > 0 &&
        LookupBuffer(flushing_buffer_, flushing_range_valid_, flushing_min_key_,
                     flushing_max_key_, lookup_key, value)) {
      return value;
    }

    if (FindInLipp(lookup_key, value)) {
      return value;
    }

    return util::OVERFLOW;
  }

  uint64_t RangeQuery(const KeyType& lower_key, const KeyType& upper_key,
                      uint32_t thread_id) const {
    (void) thread_id;

    if (immediate_lipp_mode_) {
      uint64_t result = 0;
      auto it = lipp_.lower_bound(lower_key);
      while (it != lipp_.end() && it->comp.data.key <= upper_key) {
        result += it->comp.data.value;
        ++it;
      }
      return result;
    }

    uint64_t result = 0;

    if (partitioned_read_mode_) {
      result += RangeQueryPartitionedBuffers(lower_key, upper_key);
      result += RangeQueryLippUnlocked(lower_key, upper_key);
      return result;
    }

    if (deferred_write_mode_) {
      if (active_buffer_ && active_buffered_keys_ > 0) {
        auto it = active_buffer_->lower_bound(lower_key);
        while (it != active_buffer_->end() && it->key() <= upper_key) {
          result += it->value();
          ++it;
        }
      }
      result += RangeQueryLippUnlocked(lower_key, upper_key);
      return result;
    }

    if (active_buffer_ &&
        RangeIntersectsBuffer(active_range_valid_, active_min_key_, active_max_key_,
                              lower_key, upper_key)) {
      const KeyType active_start =
          lower_key > active_min_key_ ? lower_key : active_min_key_;
      auto it = active_buffer_->lower_bound(active_start);
      while (it != active_buffer_->end() && it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    if (flushing_buffer_ &&
        RangeIntersectsBuffer(flushing_range_valid_, flushing_min_key_,
                              flushing_max_key_, lower_key, upper_key)) {
      const KeyType flush_start =
          lower_key > flushing_min_key_ ? lower_key : flushing_min_key_;
      auto it = flushing_buffer_->lower_bound(flush_start);
      while (it != flushing_buffer_->end() && it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }

    result += RangeQueryLipp(lower_key, upper_key);

    return result;
  }

  void Insert(const KeyValue<KeyType>& data, uint32_t thread_id) {
    (void) thread_id;
    if (immediate_lipp_mode_) {
      lipp_.insert(data.key, data.value);
      return;
    }
    if (partitioned_read_mode_) {
      InsertPartitioned(data);
      return;
    }
    ensure_initialized();
    active_buffer_->insert(data.key, data.value);
    ++active_buffered_keys_;
    if (deferred_write_mode_) {
      return;
    }
    ObserveActiveKey(data.key);
    if (active_buffered_keys_ < flush_threshold_keys_) {
      return;
    }
    RotateBuffersAtThreshold();
  }

  std::string name() const { return "HybridPGMLIPPAsync"; }

  std::size_t size() const {
    if (!immediate_lipp_mode_) {
      FinishPendingFlushes();
    }
    return lipp_.index_size();
  }

  bool applicable(bool unique, bool range_query, bool insert, bool multithread,
                  const std::string& ops_filename) const {
    (void) range_query;
    (void) insert;
    (void) ops_filename;
    return unique && !multithread;
  }

  std::vector<std::string> variants() const {
    if (partitioned_read_mode_) {
      return {SearchClass::name(), std::to_string(pgm_error),
              std::to_string(flush_threshold_keys_) + ProbeModeSuffix() +
                  std::to_string(partitioned_shards_) + "p"};
    }
    return {SearchClass::name(), std::to_string(pgm_error),
            std::to_string(flush_threshold_keys_) + ProbeModeSuffix()};
  }

 private:
  using BufferIndex = DynamicPGMIndex<
      KeyType, uint64_t, SearchClass,
      PGMIndex<KeyType, SearchClass, pgm_error, 16>>;
  using CoreLIPP = LIPP<KeyType, uint64_t>;

  enum class ProbeMode { kAuto, kReadOptimized, kWriteOptimized, kExactLipp };

  static constexpr size_t kDefaultFlushThreshold = 100000;
  static constexpr size_t kWriteOptimizedThreshold = 1000000;
  static constexpr size_t kDeferredWriteFlushThreshold = 2000000;

  static ProbeMode ParseProbeMode(const std::vector<int>& params) {
    if (params.size() < 2) {
      return ProbeMode::kAuto;
    }
    if (params[1] <= 0) {
      return ProbeMode::kAuto;
    }
    if (params[1] == 1) {
      return ProbeMode::kReadOptimized;
    }
    if (params[1] == 3) {
      return ProbeMode::kExactLipp;
    }
    return ProbeMode::kWriteOptimized;
  }

  static size_t ParsePartitionedShardCount(const std::vector<int>& params) {
    if (params.size() < 3) {
      return 0;
    }
    return static_cast<size_t>(std::max(params[2], 0));
  }

  std::string ProbeModeSuffix() const {
    switch (probe_mode_) {
      case ProbeMode::kReadOptimized:
        return "r";
      case ProbeMode::kWriteOptimized:
        return "w";
      case ProbeMode::kExactLipp:
        return "x";
      case ProbeMode::kAuto:
      default:
        return "a";
    }
  }

  void ensure_initialized() {
    if (!active_buffer_) {
      active_buffer_ = std::make_unique<BufferIndex>();
    }
  }

  static bool BufferMayContain(bool range_valid, const KeyType& min_key,
                               const KeyType& max_key,
                               const KeyType& lookup_key) {
    return range_valid && lookup_key >= min_key && lookup_key <= max_key;
  }

  static bool LookupBuffer(const std::unique_ptr<BufferIndex>& buffer,
                           bool range_valid, const KeyType& min_key,
                           const KeyType& max_key, const KeyType& lookup_key,
                           uint64_t& value) {
    if (!buffer || !BufferMayContain(range_valid, min_key, max_key, lookup_key)) {
      return false;
    }

    auto it = buffer->find(lookup_key);
    if (it == buffer->end()) {
      return false;
    }

    value = it->value();
    return true;
  }

  static bool LookupBufferNoRange(const std::unique_ptr<BufferIndex>& buffer,
                                  const KeyType& lookup_key, uint64_t& value) {
    if (!buffer) {
      return false;
    }

    auto it = buffer->find(lookup_key);
    if (it == buffer->end()) {
      return false;
    }

    value = it->value();
    return true;
  }

  static bool RangeIntersectsBuffer(bool range_valid, const KeyType& min_key,
                                    const KeyType& max_key,
                                    const KeyType& lower_key,
                                    const KeyType& upper_key) {
    return range_valid && upper_key >= min_key && lower_key <= max_key;
  }

  bool PreferReadOptimizedProbeOrder() const {
    if (probe_mode_ == ProbeMode::kReadOptimized) {
      return true;
    }
    if (probe_mode_ == ProbeMode::kWriteOptimized) {
      return false;
    }
    return flush_threshold_keys_ < kWriteOptimizedThreshold;
  }

  bool HasBufferedKeys() const {
    return active_buffered_keys_ > 0 || flushing_buffered_keys_ > 0;
  }

  bool BackgroundFlushActive() const {
    return background_flush_thread_.joinable() &&
           !background_flush_done_.load(std::memory_order_acquire);
  }

  bool FindInLipp(const KeyType& lookup_key, uint64_t& value) const {
    if (!BackgroundFlushActive()) {
      return lipp_.find(lookup_key, value);
    }

    std::shared_lock<std::shared_mutex> lock(lipp_mutex_);
    return lipp_.find(lookup_key, value);
  }

  uint64_t RangeQueryLipp(const KeyType& lower_key,
                          const KeyType& upper_key) const {
    auto scan_lipp = [&] {
      return RangeQueryLippUnlocked(lower_key, upper_key);
    };

    if (!BackgroundFlushActive()) {
      return scan_lipp();
    }

    std::shared_lock<std::shared_mutex> lock(lipp_mutex_);
    return scan_lipp();
  }

  uint64_t RangeQueryLippUnlocked(const KeyType& lower_key,
                                  const KeyType& upper_key) const {
    uint64_t result = 0;
    auto it = lipp_.lower_bound(lower_key);
    while (it != lipp_.end() && it->comp.data.key <= upper_key) {
      result += it->comp.data.value;
      ++it;
    }
    return result;
  }

  void ObserveActiveKey(const KeyType& key) const {
    if (!active_range_valid_) {
      active_min_key_ = key;
      active_max_key_ = key;
      active_range_valid_ = true;
      return;
    }
    if (key < active_min_key_) {
      active_min_key_ = key;
    }
    if (key > active_max_key_) {
      active_max_key_ = key;
    }
  }

  void RotateBuffersAtThreshold() const {
    if (flushing_buffer_) {
      JoinFinishedFlush();
    }

    if (flushing_buffer_) {
      return;
    }

    flushing_buffer_ = std::move(active_buffer_);
    flushing_buffered_keys_ = active_buffered_keys_;
    flushing_min_key_ = active_min_key_;
    flushing_max_key_ = active_max_key_;
    flushing_range_valid_ = active_range_valid_;

    active_buffer_ = std::make_unique<BufferIndex>();
    active_buffered_keys_ = 0;
    active_range_valid_ = false;
    StartBackgroundFlush();
  }

  void ObserveRoutingRange(const std::vector<KeyValue<KeyType>>& data) const {
    if (data.empty()) {
      routing_range_valid_ = false;
      return;
    }
    routing_min_key_ = data.front().key;
    routing_max_key_ = data.front().key;
    for (const auto& item : data) {
      if (item.key < routing_min_key_) {
        routing_min_key_ = item.key;
      }
      if (item.key > routing_max_key_) {
        routing_max_key_ = item.key;
      }
    }
    using UnsignedKey =
        typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;
    const auto span =
        static_cast<UnsignedKey>(routing_max_key_) -
        static_cast<UnsignedKey>(routing_min_key_);
    routing_scale_ =
        static_cast<long double>(partitioned_shards_) /
        (static_cast<long double>(span) + 1.0L);
    routing_range_valid_ = true;
  }

  void InitializePartitionedBuffers() const {
    partitioned_buffers_.clear();
    partitioned_buffers_.reserve(partitioned_shards_);
    for (size_t i = 0; i < partitioned_shards_; ++i) {
      partitioned_buffers_.push_back(std::make_unique<BufferIndex>());
    }
    partitioned_counts_.assign(partitioned_shards_, 0);
    partitioned_min_keys_.assign(partitioned_shards_, KeyType{});
    partitioned_max_keys_.assign(partitioned_shards_, KeyType{});
    partitioned_range_valid_.assign(partitioned_shards_, false);
    partitioned_buffered_keys_ = 0;
  }

  void ResetPartitionedBuffers() const {
    partitioned_buffers_.clear();
    partitioned_counts_.clear();
    partitioned_min_keys_.clear();
    partitioned_max_keys_.clear();
    partitioned_range_valid_.clear();
    partitioned_buffered_keys_ = 0;
  }

  size_t PartitionForKey(const KeyType& key) const {
    if (partitioned_shards_ <= 1 || !routing_range_valid_) {
      return 0;
    }
    if (key <= routing_min_key_) {
      return 0;
    }
    if (key >= routing_max_key_) {
      return partitioned_shards_ - 1;
    }

    using UnsignedKey =
        typename std::make_unsigned<typename std::remove_cv<KeyType>::type>::type;
    const auto offset =
        static_cast<UnsignedKey>(key) -
        static_cast<UnsignedKey>(routing_min_key_);
    const auto shard =
        static_cast<size_t>(static_cast<long double>(offset) * routing_scale_);
    return shard >= partitioned_shards_ ? partitioned_shards_ - 1 : shard;
  }

  void ObservePartitionKey(size_t shard, const KeyType& key) const {
    if (!partitioned_range_valid_[shard]) {
      partitioned_min_keys_[shard] = key;
      partitioned_max_keys_[shard] = key;
      partitioned_range_valid_[shard] = true;
      return;
    }
    if (key < partitioned_min_keys_[shard]) {
      partitioned_min_keys_[shard] = key;
    }
    if (key > partitioned_max_keys_[shard]) {
      partitioned_max_keys_[shard] = key;
    }
  }

  void InsertPartitioned(const KeyValue<KeyType>& data) const {
    if (partitioned_buffers_.empty()) {
      InitializePartitionedBuffers();
    }
    const size_t shard = PartitionForKey(data.key);
    partitioned_buffers_[shard]->insert(data.key, data.value);
    ++partitioned_counts_[shard];
    ++partitioned_buffered_keys_;
    ObservePartitionKey(shard, data.key);
  }

  bool LookupPartitionedBuffer(const KeyType& lookup_key,
                               uint64_t& value) const {
    if (partitioned_buffers_.empty()) {
      return false;
    }
    const size_t shard = PartitionForKey(lookup_key);
    if (shard >= partitioned_buffers_.size() || partitioned_counts_[shard] == 0 ||
        !BufferMayContain(partitioned_range_valid_[shard],
                          partitioned_min_keys_[shard],
                          partitioned_max_keys_[shard], lookup_key)) {
      return false;
    }
    auto it = partitioned_buffers_[shard]->find(lookup_key);
    if (it == partitioned_buffers_[shard]->end()) {
      return false;
    }
    value = it->value();
    return true;
  }

  uint64_t RangeQueryPartitionedBuffers(const KeyType& lower_key,
                                        const KeyType& upper_key) const {
    uint64_t result = 0;
    for (size_t shard = 0; shard < partitioned_buffers_.size(); ++shard) {
      if (partitioned_counts_[shard] == 0 ||
          !RangeIntersectsBuffer(partitioned_range_valid_[shard],
                                 partitioned_min_keys_[shard],
                                 partitioned_max_keys_[shard], lower_key,
                                 upper_key)) {
        continue;
      }
      const KeyType scan_start =
          lower_key > partitioned_min_keys_[shard]
              ? lower_key
              : partitioned_min_keys_[shard];
      auto it = partitioned_buffers_[shard]->lower_bound(scan_start);
      while (it != partitioned_buffers_[shard]->end() &&
             it->key() <= upper_key) {
        result += it->value();
        ++it;
      }
    }
    return result;
  }

  static bool AuditEnabled() {
    static const bool enabled = std::getenv("HYBRID_PGM_LIPP_AUDIT") != nullptr;
    return enabled;
  }

  static size_t CountBufferEntries(const std::unique_ptr<BufferIndex>& buffer) {
    if (!buffer) {
      return 0;
    }
    size_t count = 0;
    auto it = buffer->lower_bound(std::numeric_limits<KeyType>::lowest());
    while (it != buffer->end()) {
      ++count;
      ++it;
    }
    return count;
  }

  static void AuditBufferCount(const std::unique_ptr<BufferIndex>& buffer,
                               size_t expected, const char* label) {
    if (!AuditEnabled()) {
      return;
    }
    const size_t actual = CountBufferEntries(buffer);
    if (actual != expected) {
      util::fail(std::string("HybridPGMLIPPAsync audit failed for ") + label +
                 ": expected " + std::to_string(expected) + " buffered keys, saw " +
                 std::to_string(actual));
    }
  }

  void AuditPartitionedBufferCount(size_t shard) const {
    if (!AuditEnabled()) {
      return;
    }
    const size_t actual = CountBufferEntries(partitioned_buffers_[shard]);
    if (actual != partitioned_counts_[shard]) {
      util::fail("HybridPGMLIPPAsync partition audit failed for shard " +
                 std::to_string(shard) + ": expected " +
                 std::to_string(partitioned_counts_[shard]) +
                 " buffered keys, saw " + std::to_string(actual));
    }
  }

  size_t DrainBufferIntoLipp(const std::unique_ptr<BufferIndex>& buffer,
                             size_t expected_count,
                             const char* label) const {
    if (!buffer) {
      AuditBufferCount(buffer, expected_count, label);
      return 0;
    }

    AuditBufferCount(buffer, expected_count, label);

    std::unique_lock<std::shared_mutex> lock(lipp_mutex_);
    size_t drained_count = 0;
    auto it = buffer->lower_bound(std::numeric_limits<KeyType>::lowest());
    while (it != buffer->end()) {
      lipp_.insert(it->key(), it->value());
      ++drained_count;
      ++it;
    }
    if (AuditEnabled() && drained_count != expected_count) {
      util::fail(std::string("HybridPGMLIPPAsync drain audit failed for ") + label +
                 ": expected " + std::to_string(expected_count) +
                 " drained keys, saw " + std::to_string(drained_count));
    }
    return drained_count;
  }

  void FinishPendingFlushes() const {
    if (immediate_lipp_mode_) {
      return;
    }

    if (partitioned_read_mode_) {
      DrainPartitionedBuffersIntoLipp();
      InitializePartitionedBuffers();
      return;
    }

    WaitForBackgroundFlush();
    DrainBufferIntoLipp(flushing_buffer_, flushing_buffered_keys_,
                        "foreground flushing buffer");
    DrainBufferIntoLipp(active_buffer_, active_buffered_keys_,
                        "active buffer");

    flushing_buffer_.reset();
    active_buffer_ = std::make_unique<BufferIndex>();
    flushing_buffered_keys_ = 0;
    active_buffered_keys_ = 0;
    flushing_range_valid_ = false;
    active_range_valid_ = false;
  }

  void DrainPartitionedBuffersIntoLipp() const {
    if (partitioned_buffers_.empty() || partitioned_buffered_keys_ == 0) {
      return;
    }

    std::unique_lock<std::shared_mutex> lock(lipp_mutex_);
    size_t total_drained = 0;
    for (size_t shard = 0; shard < partitioned_buffers_.size(); ++shard) {
      if (partitioned_counts_[shard] == 0) {
        AuditPartitionedBufferCount(shard);
        continue;
      }
      AuditPartitionedBufferCount(shard);
      size_t shard_drained = 0;
      auto it =
          partitioned_buffers_[shard]->lower_bound(std::numeric_limits<KeyType>::lowest());
      while (it != partitioned_buffers_[shard]->end()) {
        lipp_.insert(it->key(), it->value());
        ++shard_drained;
        ++total_drained;
        ++it;
      }
      if (AuditEnabled() && shard_drained != partitioned_counts_[shard]) {
        util::fail("HybridPGMLIPPAsync partition drain audit failed for shard " +
                   std::to_string(shard) + ": expected " +
                   std::to_string(partitioned_counts_[shard]) +
                   " drained keys, saw " + std::to_string(shard_drained));
      }
    }
    if (AuditEnabled() && total_drained != partitioned_buffered_keys_) {
      util::fail("HybridPGMLIPPAsync partition total audit failed: expected " +
                 std::to_string(partitioned_buffered_keys_) +
                 " drained keys, saw " + std::to_string(total_drained));
    }
  }

  void StartBackgroundFlush() const {
    if (!flushing_buffer_) {
      background_flush_done_.store(true, std::memory_order_release);
      return;
    }

    background_flush_done_.store(false, std::memory_order_release);
    background_flush_expected_keys_ = flushing_buffered_keys_;
    background_flush_thread_ = std::thread([this] {
      background_flush_drained_keys_ =
          DrainBufferIntoLipp(flushing_buffer_, background_flush_expected_keys_,
                              "background flushing buffer");
      background_flush_done_.store(true, std::memory_order_release);
    });
  }

  void JoinFinishedFlush() const {
    if (!background_flush_thread_.joinable()) {
      return;
    }
    if (!background_flush_done_.load(std::memory_order_acquire)) {
      return;
    }

    background_flush_thread_.join();
    AuditCompletedBackgroundFlush();
    flushing_buffer_.reset();
    flushing_buffered_keys_ = 0;
    flushing_range_valid_ = false;
  }

  void WaitForBackgroundFlush() const {
    if (background_flush_thread_.joinable()) {
      background_flush_thread_.join();
      AuditCompletedBackgroundFlush();
      flushing_buffer_.reset();
      flushing_buffered_keys_ = 0;
      flushing_range_valid_ = false;
    }
  }

  void AuditCompletedBackgroundFlush() const {
    if (!AuditEnabled()) {
      return;
    }
    if (background_flush_drained_keys_ != background_flush_expected_keys_) {
      util::fail("HybridPGMLIPPAsync background audit failed: expected " +
                 std::to_string(background_flush_expected_keys_) +
                 " drained keys, saw " +
                 std::to_string(background_flush_drained_keys_));
    }
  }

  size_t flush_threshold_keys_;
  ProbeMode probe_mode_;
  size_t partitioned_shards_;
  bool partitioned_read_mode_;
  bool immediate_lipp_mode_;
  bool deferred_write_mode_;
  mutable size_t active_buffered_keys_ = 0;
  mutable size_t flushing_buffered_keys_ = 0;
  mutable KeyType active_min_key_{};
  mutable KeyType active_max_key_{};
  mutable KeyType flushing_min_key_{};
  mutable KeyType flushing_max_key_{};
  mutable bool active_range_valid_ = false;
  mutable bool flushing_range_valid_ = false;
  mutable std::shared_mutex lipp_mutex_;
  mutable std::thread background_flush_thread_;
  mutable std::atomic<bool> background_flush_done_{true};
  mutable size_t background_flush_expected_keys_ = 0;
  mutable size_t background_flush_drained_keys_ = 0;
  mutable CoreLIPP lipp_;
  mutable std::unique_ptr<BufferIndex> active_buffer_;
  mutable std::unique_ptr<BufferIndex> flushing_buffer_;
  mutable std::vector<std::unique_ptr<BufferIndex>> partitioned_buffers_;
  mutable std::vector<size_t> partitioned_counts_;
  mutable std::vector<KeyType> partitioned_min_keys_;
  mutable std::vector<KeyType> partitioned_max_keys_;
  mutable std::vector<bool> partitioned_range_valid_;
  mutable size_t partitioned_buffered_keys_ = 0;
  mutable KeyType routing_min_key_{};
  mutable KeyType routing_max_key_{};
  mutable long double routing_scale_ = 0.0L;
  mutable bool routing_range_valid_ = false;
};

#endif  // TLI_HYBRID_PGM_LIPP_H
