#include "ReplicationApplier.h"

#include <algorithm>
#include <stdexcept>

namespace arangodb {

ReplicationApplier::ReplicationApplier(WriteCallback writeCb,
                                       SatelliteCheckCallback satelliteCb)
    : _writeCb(std::move(writeCb)), _satelliteCb(std::move(satelliteCb)) {
  if (!_writeCb) {
    throw std::invalid_argument(
        "ReplicationApplier requires a write callback");
  }
  if (!_satelliteCb) {
    throw std::invalid_argument(
        "ReplicationApplier requires a satellite check callback");
  }
}

ReplicationApplier::~ReplicationApplier() = default;

uint64_t ReplicationApplier::applyBatch(
    std::vector<ApplyMessage> const& messages) {
  std::lock_guard<std::mutex> guard(_mutex);
  uint64_t appliedBefore = _totalApplied;
  for (auto const& msg : messages) {
    int rc = applyLocked(msg);
    if (rc != 0) {
      // Propagate write errors but continue counting what was applied.
      break;
    }
  }
  return _totalApplied - appliedBefore;
}

int ReplicationApplier::applyMessage(ApplyMessage const& message) {
  std::lock_guard<std::mutex> guard(_mutex);
  return applyLocked(message);
}

int ReplicationApplier::applyLocked(ApplyMessage const& message) {
  // Skip satellite collections.
  if (_satelliteCb(message.shardId)) {
    ++_totalSatelliteSkipped;
    return 0;
  }

  uint64_t lastSeq = 0;
  auto it = _lastApplied.find(message.shardId);
  if (it != _lastApplied.end()) {
    lastSeq = it->second;
  }

  // Reject duplicates (idempotency).
  if (message.sequence <= lastSeq) {
    ++_totalDuplicates;
    return 0;
  }

  // Check if this is the next expected sequence.
  uint64_t expectedSeq = lastSeq + 1;
  if (message.sequence != expectedSeq) {
    // Out-of-order: buffer for later.
    _buffer[message.shardId][message.sequence] = message;
    return 0;
  }

  // Apply the message via the write callback.
  int rc = _writeCb(message.shardId, message.operation, message.payload,
                     message.documentKey);
  if (rc != 0) {
    return rc;
  }

  // Update tracking.
  _lastApplied[message.shardId] = message.sequence;
  ++_totalApplied;

  // Drain any buffered messages that are now in-order.
  drainBuffered(message.shardId);

  return 0;
}

uint64_t ReplicationApplier::lastAppliedSequence(
    std::string const& shardId) const {
  std::lock_guard<std::mutex> guard(_mutex);
  auto it = _lastApplied.find(shardId);
  return (it != _lastApplied.end()) ? it->second : 0;
}

uint64_t ReplicationApplier::bufferedCount() const {
  std::lock_guard<std::mutex> guard(_mutex);
  uint64_t count = 0;
  for (auto const& [shard, buf] : _buffer) {
    count += buf.size();
  }
  return count;
}

uint64_t ReplicationApplier::drainBuffered(std::string const& shardId) {
  // NOTE: called from applyLocked which already holds _mutex.
  // Do NOT re-acquire the lock here.
  uint64_t drained = 0;
  auto bufIt = _buffer.find(shardId);
  if (bufIt == _buffer.end()) {
    return 0;
  }

  auto& shardBuf = bufIt->second;
  while (!shardBuf.empty()) {
    uint64_t nextSeq = _lastApplied[shardId] + 1;
    auto msgIt = shardBuf.find(nextSeq);
    if (msgIt == shardBuf.end()) {
      break;  // gap still exists
    }

    auto const& msg = msgIt->second;
    int rc =
        _writeCb(msg.shardId, msg.operation, msg.payload, msg.documentKey);
    if (rc != 0) {
      break;
    }

    _lastApplied[shardId] = nextSeq;
    ++_totalApplied;
    ++drained;
    shardBuf.erase(msgIt);
  }

  if (shardBuf.empty()) {
    _buffer.erase(bufIt);
  }

  return drained;
}

}  // namespace arangodb
