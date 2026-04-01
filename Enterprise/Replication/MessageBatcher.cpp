#include "MessageBatcher.h"

#include <chrono>
#include <utility>

namespace arangodb {

MessageBatcher::MessageBatcher(std::string shardId,
                               uint32_t batchSize,
                               SequenceProvider sequenceProvider)
    : _shardId(std::move(shardId)),
      _batchSize(batchSize),
      _sequenceProvider(std::move(sequenceProvider)) {
  _pending.reserve(_batchSize);
}

std::optional<MessageBatch> MessageBatcher::add(WALEntry entry) {
  _pending.push_back(std::move(entry));

  if (_pending.size() >= _batchSize) {
    return seal();
  }
  return std::nullopt;
}

std::optional<MessageBatch> MessageBatcher::flush() {
  if (_pending.empty()) {
    return std::nullopt;
  }
  return seal();
}

void MessageBatcher::reset() {
  _pending.clear();
}

uint32_t MessageBatcher::pendingCount() const {
  return static_cast<uint32_t>(_pending.size());
}

MessageBatch MessageBatcher::seal() {
  auto now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();

  MessageBatch msg;
  msg.shardId = _shardId;
  msg.sequenceNumber = _sequenceProvider();
  msg.entries = std::move(_pending);
  msg.batchTimestamp = static_cast<uint64_t>(now);

  _pending.clear();
  _pending.reserve(_batchSize);

  return msg;
}

}  // namespace arangodb
