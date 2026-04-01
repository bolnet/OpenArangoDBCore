#pragma once
#ifndef ARANGODB_ROCKSDB_BUILDER_INDEX_EE_H
#define ARANGODB_ROCKSDB_BUILDER_INDEX_EE_H

#include "Enterprise/RocksDBEngine/ChangelogBuffer.h"
#include "Enterprise/RocksDBEngine/IndexBuilderThreadPool.h"
#include "Enterprise/RocksDBEngine/KeySpacePartitioner.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace arangodb {

/// Configuration for parallel index building.
struct IndexBuilderConfig {
  uint32_t numThreads = 4;
  uint64_t memoryBudgetMB = 256;
  bool backgroundMode = false;
};

/// Abstract iterator interface for index building.
/// Each thread gets its own iterator bound to the shared snapshot.
class IndexIterator {
 public:
  virtual ~IndexIterator() = default;
  virtual void seek(std::string const& key) = 0;
  virtual bool valid() const = 0;
  virtual void next() = 0;
  virtual std::string key() const = 0;
  virtual std::string value() const = 0;
};

/// Enterprise parallel index builder.
/// Wraps/extends Community RocksDBIndex::fillIndex() with:
///   - Key space partitioning across configurable thread pool
///   - Per-thread RocksDB iterators at same MVCC snapshot
///   - Background mode with shared lock and changelog buffer
///   - Atomic index swap on completion
class RocksDBBuilderIndexEE {
 public:
  /// Build lifecycle state machine.
  enum class BuildState : uint8_t {
    kIdle = 0,
    kBuilding = 1,
    kApplyingChangelog = 2,
    kSwapping = 3,
    kComplete = 4,
    kFailed = 5
  };

  explicit RocksDBBuilderIndexEE(IndexBuilderConfig config);
  ~RocksDBBuilderIndexEE();

  // Non-copyable, non-movable
  RocksDBBuilderIndexEE(RocksDBBuilderIndexEE const&) = delete;
  RocksDBBuilderIndexEE& operator=(RocksDBBuilderIndexEE const&) = delete;

  /// Fill the index in parallel. This is the enterprise replacement for
  /// the community single-threaded RocksDBIndex::fillIndex().
  ///
  /// @param snapshotFactory  Creates a per-thread iterator bound to the
  ///                         shared MVCC snapshot for a given key range.
  /// @param indexInserter    Callback to insert a key-value pair into the
  ///                         index (must be thread-safe or partitioned).
  /// @param keyRangeLower    Lower bound of the key space (inclusive).
  /// @param keyRangeUpper    Upper bound of the key space (exclusive,
  ///                         empty=unbounded).
  /// @return true on success, false on failure.
  bool fillIndexParallel(
      std::function<std::unique_ptr<IndexIterator>(
          std::string const& lowerBound,
          std::string const& upperBound)> snapshotFactory,
      std::function<bool(std::string const& key,
                         std::string const& value)> indexInserter,
      std::string const& keyRangeLower,
      std::string const& keyRangeUpper);

  /// Register a concurrent write that occurred during background build.
  /// Only valid when backgroundMode is true and state is kBuilding.
  bool bufferWrite(ChangelogEntry entry);

  /// Apply buffered changelog to the index after parallel fill completes.
  bool applyChangelog(
      std::function<bool(ChangelogEntry const&)> const& applier);

  /// Perform atomic swap: publish the newly built index and discard the old.
  /// Returns false if the build did not complete successfully.
  bool atomicSwap(std::function<void()> swapCallback);

  /// Query the current build state.
  BuildState state() const;

  /// Access the configuration.
  IndexBuilderConfig const& config() const;

 private:
  IndexBuilderConfig _config;
  std::unique_ptr<IndexBuilderThreadPool> _threadPool;
  std::unique_ptr<ChangelogBuffer> _changelog;
  std::atomic<BuildState> _state;
};

}  // namespace arangodb
#endif  // ARANGODB_ROCKSDB_BUILDER_INDEX_EE_H
