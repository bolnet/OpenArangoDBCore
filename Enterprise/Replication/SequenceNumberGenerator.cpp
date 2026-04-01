#include "SequenceNumberGenerator.h"

namespace arangodb {

uint64_t SequenceNumberGenerator::nextSequence(std::string const& shardId) {
  auto counter = getOrCreate(shardId);
  // fetch_add returns the value BEFORE increment; add 1 so sequences
  // start at 1 (0 is reserved as the "no sequence" sentinel).
  return counter->fetch_add(1, std::memory_order_relaxed) + 1;
}

uint64_t SequenceNumberGenerator::currentSequence(
    std::string const& shardId) const {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  auto it = _counters.find(shardId);
  if (it == _counters.end()) {
    return 0;
  }
  return it->second->load(std::memory_order_relaxed);
}

void SequenceNumberGenerator::reset() {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _counters.clear();
}

std::shared_ptr<std::atomic<uint64_t>>
SequenceNumberGenerator::getOrCreate(std::string const& shardId) {
  // Fast path: shard already exists (read lock only).
  {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _counters.find(shardId);
    if (it != _counters.end()) {
      return it->second;
    }
  }
  // Slow path: insert new counter (write lock).
  std::unique_lock<std::shared_mutex> lock(_mutex);
  auto [it, inserted] = _counters.emplace(
      shardId, std::make_shared<std::atomic<uint64_t>>(0));
  return it->second;
}

}  // namespace arangodb
