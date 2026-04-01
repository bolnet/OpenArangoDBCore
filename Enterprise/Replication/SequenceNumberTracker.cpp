#include "SequenceNumberTracker.h"
#include <algorithm>
#include <mutex>

namespace arangodb {

bool SequenceNumberTracker::isAlreadyApplied(
    std::string const& shardId, uint64_t seq) const {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  auto it = _lastApplied.find(shardId);
  if (it == _lastApplied.end()) {
    return false;  // Shard never seen -- message is new.
  }
  return seq <= it->second;
}

void SequenceNumberTracker::markApplied(
    std::string const& shardId, uint64_t seq) {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  auto [it, inserted] = _lastApplied.emplace(shardId, seq);
  if (!inserted) {
    it->second = std::max(it->second, seq);
  }
}

uint64_t SequenceNumberTracker::lastAppliedSequence(
    std::string const& shardId) const {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  auto it = _lastApplied.find(shardId);
  if (it == _lastApplied.end()) {
    return 0;
  }
  return it->second;
}

std::unordered_map<std::string, uint64_t>
SequenceNumberTracker::getState() const {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _lastApplied;  // Return a copy (snapshot).
}

void SequenceNumberTracker::restoreState(
    std::unordered_map<std::string, uint64_t> const& state) {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _lastApplied = state;
}

void SequenceNumberTracker::reset() {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _lastApplied.clear();
}

}  // namespace arangodb
