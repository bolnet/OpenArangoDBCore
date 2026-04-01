#pragma once
#ifndef ARANGODB_DC2DC_REPLICATOR_H
#define ARANGODB_DC2DC_REPLICATOR_H

#include "DirectMQMessage.h"
#include "IdempotencyChecker.h"
#include "IWALIterator.h"
#include "MessageBatcher.h"
#include "ReplicationCheckpoint.h"
#include "ReplicationLagMonitor.h"
#include "SequenceNumberGenerator.h"
#include "SequenceNumberTracker.h"
#include "ShardWALTailer.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace arangodb {

/// Configuration options for DC2DC replication.
struct DC2DCOptions {
  bool enabled = false;
  std::string targetCluster;              // URL of target coordinator
  uint32_t lagToleranceSec = 30;          // Max acceptable replication lag
  uint32_t batchSize = 1000;              // Documents per DirectMQ message
  uint32_t threadCount = 4;               // Number of shard tail threads
  uint32_t maxRetries = 5;               // Network retry attempts
  uint32_t checkpointIntervalSec = 60;    // Checkpoint persistence interval
  std::string checkpointPath = "replication-checkpoint.json";
};

/// Factory that creates an IWALIterator for a given shard.
/// Production code returns a RocksDB WAL iterator; tests return MockWALIterator.
using WALIteratorFactory =
    std::function<std::unique_ptr<IWALIterator>(uint32_t shardIndex)>;

/// Callback invoked when a sealed MessageBatch is ready for transport.
/// The transport layer (05-03) will provide the real implementation.
using BatchReadyCallback = std::function<void(MessageBatch)>;

/// DC2DCReplicator manages cross-datacenter replication as an
/// ApplicationFeature integrated into the ArangoDB server lifecycle.
///
/// Lifecycle:
///   collectOptions() -> validateOptions() -> prepare() -> start() -> stop()
///
/// Phase 05-01 provides the skeleton, sequence/idempotency components.
/// Phase 05-02 adds WAL tailing, message batching, and lag monitoring.
class DC2DCReplicator {
 public:
  DC2DCReplicator();
  ~DC2DCReplicator();

  // ApplicationFeature lifecycle methods.

  /// Register configuration options (--replication.dc2dc.*).
  void collectOptions();

  /// Validate options (e.g., target-cluster required when enabled).
  /// Returns true on success, false on validation failure.
  bool validateOptions();

  /// Initialize internal components (generator, tracker, checker).
  void prepare();

  /// Load checkpoint and begin replication.
  /// If WAL tailing has been configured via configureWALTailing(),
  /// per-shard tail threads are started.
  void start();

  /// Persist checkpoint, stop WAL tailing threads, and shut down gracefully.
  void stop();

  // --- WAL Tailing configuration (05-02) ---

  /// Configure WAL tailing parameters. Must be called before start().
  /// @param numberOfShards  Total number of shards to tail
  /// @param iteratorFactory Creates an IWALIterator per shard
  /// @param collectionResolver Resolves collection replication info
  /// @param sequenceProvider Provides monotonic sequence numbers for batches
  /// @param onBatchReady Callback when a batch is sealed
  void configureWALTailing(uint32_t numberOfShards,
                           WALIteratorFactory iteratorFactory,
                           CollectionInfoResolver collectionResolver,
                           SequenceProvider sequenceProvider,
                           BatchReadyCallback onBatchReady);

  /// Block until all WAL tailing threads have exited.
  void joinTailThreads();

  /// Check if WAL tailing threads are currently running.
  bool isTailing() const;

  /// Access the lag monitor for observability. May be null before prepare().
  ReplicationLagMonitor const* lagMonitor() const;

  // Accessors for sub-components (used by later plans and tests).

  DC2DCOptions const& options() const { return _options; }
  DC2DCOptions& mutableOptions() { return _options; }

  SequenceNumberGenerator* generator() const { return _generator.get(); }
  SequenceNumberTracker* tracker() const { return _tracker.get(); }
  IdempotencyChecker* checker() const { return _checker.get(); }
  ReplicationCheckpoint* checkpoint() const { return _checkpoint.get(); }

  /// Whether the feature has been prepared and is ready.
  bool isPrepared() const { return _prepared; }

 private:
  /// Main loop for a single shard tail thread.
  void shardTailLoop(uint32_t shardIndex);

  DC2DCOptions _options;
  bool _prepared = false;

  // 05-01 components
  std::unique_ptr<SequenceNumberGenerator> _generator;
  std::unique_ptr<SequenceNumberTracker> _tracker;
  std::unique_ptr<IdempotencyChecker> _checker;
  std::unique_ptr<ReplicationCheckpoint> _checkpoint;

  // 05-02 WAL tailing components
  bool _walTailingConfigured = false;
  uint32_t _numberOfShards = 0;
  WALIteratorFactory _iteratorFactory;
  CollectionInfoResolver _collectionResolver;
  SequenceProvider _sequenceProvider;
  BatchReadyCallback _onBatchReady;

  std::atomic<bool> _tailing{false};
  std::vector<std::thread> _tailThreads;
  std::vector<std::unique_ptr<ShardWALTailer>> _tailers;
  std::unique_ptr<ReplicationLagMonitor> _lagMonitor;
};

}  // namespace arangodb

#endif  // ARANGODB_DC2DC_REPLICATOR_H
