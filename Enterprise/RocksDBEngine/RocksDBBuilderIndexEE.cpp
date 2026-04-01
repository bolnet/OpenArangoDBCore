#include "RocksDBBuilderIndexEE.h"

#include <future>
#include <utility>
#include <vector>

namespace arangodb {

RocksDBBuilderIndexEE::RocksDBBuilderIndexEE(IndexBuilderConfig config)
    : _config(std::move(config)),
      _threadPool(std::make_unique<IndexBuilderThreadPool>(_config.numThreads)),
      _changelog(std::make_unique<ChangelogBuffer>(
          _config.memoryBudgetMB * 1024ULL * 1024ULL)),
      _state(BuildState::kIdle) {}

RocksDBBuilderIndexEE::~RocksDBBuilderIndexEE() {
  // Thread pool destructor handles shutdown and joining
}

bool RocksDBBuilderIndexEE::fillIndexParallel(
    std::function<std::unique_ptr<IndexIterator>(
        std::string const& lowerBound,
        std::string const& upperBound)> snapshotFactory,
    std::function<bool(std::string const& key,
                       std::string const& value)> indexInserter,
    std::string const& keyRangeLower,
    std::string const& keyRangeUpper) {
  // Transition: Idle -> Building
  BuildState expected = BuildState::kIdle;
  if (!_state.compare_exchange_strong(expected, BuildState::kBuilding)) {
    return false;  // Already building or in another state
  }

  // Partition key space across configured threads
  auto partitions = KeySpacePartitioner::partition(
      keyRangeLower, keyRangeUpper, _config.numThreads);

  if (partitions.empty() && _config.numThreads > 0) {
    // Empty key range, nothing to index -- that's still a success
    // State remains kBuilding (caller should applyChangelog + atomicSwap)
    return true;
  }

  // Submit one task per partition
  std::vector<std::future<bool>> futures;
  futures.reserve(partitions.size());

  for (auto const& range : partitions) {
    auto result = _threadPool->submit(
        [&snapshotFactory, &indexInserter, range]() -> bool {
          auto iterator = snapshotFactory(range.lowerBound, range.upperBound);
          if (!iterator) {
            return false;
          }

          iterator->seek(range.lowerBound);

          while (iterator->valid()) {
            std::string k = iterator->key();

            // If upperBound is non-empty, stop when we reach/exceed it
            if (!range.upperBound.empty() && k >= range.upperBound) {
              break;
            }

            std::string v = iterator->value();
            if (!indexInserter(k, v)) {
              return false;  // Insertion failed
            }

            iterator->next();
          }

          return true;
        });

    if (!result.has_value()) {
      // Thread pool rejected the task (shut down)
      _state.store(BuildState::kFailed, std::memory_order_release);
      return false;
    }

    futures.push_back(std::move(result.value()));
  }

  // Wait for all tasks and check results
  bool allSucceeded = true;
  for (auto& future : futures) {
    try {
      if (!future.get()) {
        allSucceeded = false;
      }
    } catch (...) {
      allSucceeded = false;
    }
  }

  if (!allSucceeded) {
    _state.store(BuildState::kFailed, std::memory_order_release);
    return false;
  }

  // State remains kBuilding -- caller should call applyChangelog then atomicSwap
  return true;
}

bool RocksDBBuilderIndexEE::bufferWrite(ChangelogEntry entry) {
  if (!_config.backgroundMode) {
    return false;
  }

  auto currentState = _state.load(std::memory_order_acquire);
  if (currentState != BuildState::kBuilding) {
    return false;
  }

  return _changelog->append(std::move(entry));
}

bool RocksDBBuilderIndexEE::applyChangelog(
    std::function<bool(ChangelogEntry const&)> const& applier) {
  // Transition: Building -> ApplyingChangelog
  BuildState expected = BuildState::kBuilding;
  if (!_state.compare_exchange_strong(expected, BuildState::kApplyingChangelog)) {
    return false;
  }

  bool success = true;
  _changelog->forEach([&](ChangelogEntry const& entry) {
    if (success) {
      if (!applier(entry)) {
        success = false;
      }
    }
  });

  if (!success) {
    _state.store(BuildState::kFailed, std::memory_order_release);
    return false;
  }

  _changelog->clear();
  return true;
}

bool RocksDBBuilderIndexEE::atomicSwap(std::function<void()> swapCallback) {
  // Transition: ApplyingChangelog -> Swapping
  BuildState expected = BuildState::kApplyingChangelog;
  if (!_state.compare_exchange_strong(expected, BuildState::kSwapping)) {
    return false;
  }

  try {
    swapCallback();
  } catch (...) {
    _state.store(BuildState::kFailed, std::memory_order_release);
    return false;
  }

  _state.store(BuildState::kComplete, std::memory_order_release);
  return true;
}

RocksDBBuilderIndexEE::BuildState RocksDBBuilderIndexEE::state() const {
  return _state.load(std::memory_order_acquire);
}

IndexBuilderConfig const& RocksDBBuilderIndexEE::config() const {
  return _config;
}

}  // namespace arangodb
