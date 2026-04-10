#ifndef TLI_HYBRID_PGM_LIPP_H
#define TLI_HYBRID_PGM_LIPP_H

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../util.h"
#include "PGM-index/include/pgm_index.hpp"
#include "PGM-index/include/pgm_index_dynamic.hpp"
#include "base.h"
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

#endif  // TLI_HYBRID_PGM_LIPP_H
