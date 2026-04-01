#include "DC2DCReplicator.h"

#include <chrono>
#include <stdexcept>

namespace arangodb {

DC2DCReplicator::DC2DCReplicator() = default;

DC2DCReplicator::~DC2DCReplicator() {
  // Stop WAL tailing threads if running
  if (_tailing.load()) {
    _tailing.store(false);
    for (auto& tailer : _tailers) {
      if (tailer) {
        tailer->stop();
      }
    }
  }
  joinTailThreads();
}

void DC2DCReplicator::collectOptions() {
  // In a full ApplicationFeature, these would register with ProgramOptions.
  // Option keys registered:
  //   --replication.dc2dc.enabled              (bool, default false)
  //   --replication.dc2dc.target-cluster       (string, default "")
  //   --replication.dc2dc.lag-tolerance        (uint32, default 30)
  //   --replication.dc2dc.batch-size           (uint32, default 1000)
  //   --replication.dc2dc.thread-count         (uint32, default 4)
  //   --replication.dc2dc.max-retries          (uint32, default 5)
  //   --replication.dc2dc.checkpoint-interval  (uint32, default 60)
  //
  // Actual ProgramOptions integration deferred to 05-04 REST API plan.
}

bool DC2DCReplicator::validateOptions() {
  if (!_options.enabled) {
    return true;  // Disabled -- nothing to validate.
  }
  if (_options.targetCluster.empty()) {
    return false;  // Target cluster URL is required when enabled.
  }
  if (_options.batchSize == 0) {
    return false;  // Batch size must be positive.
  }
  if (_options.threadCount == 0) {
    return false;  // Thread count must be positive.
  }
  if (_options.checkpointIntervalSec == 0) {
    return false;  // Checkpoint interval must be positive.
  }
  return true;
}

void DC2DCReplicator::prepare() {
  _generator = std::make_unique<SequenceNumberGenerator>();
  _tracker = std::make_unique<SequenceNumberTracker>();
  _checker = std::make_unique<IdempotencyChecker>(*_tracker);
  _checkpoint = std::make_unique<ReplicationCheckpoint>(
      _options.checkpointPath, *_tracker);
  _lagMonitor = std::make_unique<ReplicationLagMonitor>(
      static_cast<double>(_options.lagToleranceSec));
  _prepared = true;
}

void DC2DCReplicator::start() {
  if (!_prepared) {
    throw std::runtime_error(
        "DC2DCReplicator::start() called before prepare()");
  }

  // Load persisted checkpoint to restore tracker state.
  auto result = _checkpoint->load();
  if (!result.success) {
    throw std::runtime_error(
        "Failed to load replication checkpoint: " + result.error);
  }

  // Start WAL tailing threads if configured (05-02).
  if (_walTailingConfigured && !_tailing.load()) {
    _tailing.store(true);
    _tailers.clear();
    _tailThreads.clear();

    for (uint32_t i = 0; i < _numberOfShards; ++i) {
      _tailThreads.emplace_back(&DC2DCReplicator::shardTailLoop, this, i);
    }
  }
}

void DC2DCReplicator::stop() {
  if (!_prepared) {
    return;  // Nothing to stop if never prepared.
  }

  // Stop WAL tailing threads (05-02).
  if (_tailing.load()) {
    _tailing.store(false);
    for (auto& tailer : _tailers) {
      if (tailer) {
        tailer->stop();
      }
    }
  }

  // Persist final checkpoint before shutdown.
  auto result = _checkpoint->save();
  if (!result.success) {
    // Log error but don't throw during shutdown.
    // In production, this would use the ArangoDB logger.
  }
}

void DC2DCReplicator::configureWALTailing(
    uint32_t numberOfShards,
    WALIteratorFactory iteratorFactory,
    CollectionInfoResolver collectionResolver,
    SequenceProvider sequenceProvider,
    BatchReadyCallback onBatchReady) {
  _numberOfShards = numberOfShards;
  _iteratorFactory = std::move(iteratorFactory);
  _collectionResolver = std::move(collectionResolver);
  _sequenceProvider = std::move(sequenceProvider);
  _onBatchReady = std::move(onBatchReady);
  _walTailingConfigured = true;
}

void DC2DCReplicator::joinTailThreads() {
  for (auto& t : _tailThreads) {
    if (t.joinable()) {
      t.join();
    }
  }
  _tailThreads.clear();
  _tailers.clear();
}

bool DC2DCReplicator::isTailing() const {
  return _tailing.load();
}

ReplicationLagMonitor const* DC2DCReplicator::lagMonitor() const {
  return _lagMonitor.get();
}

void DC2DCReplicator::shardTailLoop(uint32_t shardIndex) {
  auto iterator = _iteratorFactory(shardIndex);
  auto tailer = std::make_unique<ShardWALTailer>(
      shardIndex, _numberOfShards, std::move(iterator), _collectionResolver);

  std::string shardId = "shard_" + std::to_string(shardIndex);
  MessageBatcher batcher(shardId, _options.batchSize, _sequenceProvider);

  while (_tailing.load(std::memory_order_relaxed)) {
    auto entries = tailer->poll(_options.batchSize);

    for (auto& entry : entries) {
      _lagMonitor->recordEntry(shardId, entry.timestamp);

      auto msg = batcher.add(std::move(entry));
      if (msg.has_value()) {
        _onBatchReady(std::move(msg.value()));
      }
    }

    if (entries.empty()) {
      // No new entries; flush partial batch and sleep briefly
      auto partial = batcher.flush();
      if (partial.has_value()) {
        _onBatchReady(std::move(partial.value()));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  // Flush remaining entries on shutdown
  auto remaining = batcher.flush();
  if (remaining.has_value()) {
    _onBatchReady(std::move(remaining.value()));
  }
}

}  // namespace arangodb
