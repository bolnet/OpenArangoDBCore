#include "IndexBuilderThreadPool.h"

#include <utility>

namespace arangodb {

IndexBuilderThreadPool::IndexBuilderThreadPool(uint32_t numThreads)
    : _shutdown(false),
      _numThreads(numThreads) {
  _workers.reserve(numThreads);
  for (uint32_t i = 0; i < numThreads; ++i) {
    _workers.emplace_back(&IndexBuilderThreadPool::workerLoop, this);
  }
}

IndexBuilderThreadPool::~IndexBuilderThreadPool() {
  shutdown();
}

void IndexBuilderThreadPool::workerLoop() {
  while (true) {
    std::packaged_task<bool()> task;

    {
      std::unique_lock<std::mutex> lock(_mutex);
      _cv.wait(lock, [this] {
        return _shutdown.load(std::memory_order_relaxed) || !_taskQueue.empty();
      });

      if (_shutdown.load(std::memory_order_relaxed) && _taskQueue.empty()) {
        return;
      }

      if (_taskQueue.empty()) {
        continue;
      }

      task = std::move(_taskQueue.front());
      _taskQueue.pop();
    }

    task();
  }
}

std::optional<std::future<bool>> IndexBuilderThreadPool::submit(
    std::function<bool()> task) {
  if (_shutdown.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  std::packaged_task<bool()> packagedTask(std::move(task));
  auto future = packagedTask.get_future();

  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_shutdown.load(std::memory_order_relaxed)) {
      return std::nullopt;
    }
    _taskQueue.push(std::move(packagedTask));
  }
  _cv.notify_one();

  return future;
}

void IndexBuilderThreadPool::shutdown() {
  bool expected = false;
  if (!_shutdown.compare_exchange_strong(expected, true,
                                         std::memory_order_release)) {
    // Already shut down, but still need to join threads if they exist
    for (auto& worker : _workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    return;
  }

  _cv.notify_all();

  for (auto& worker : _workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

uint32_t IndexBuilderThreadPool::numThreads() const {
  return _numThreads;
}

bool IndexBuilderThreadPool::isRunning() const {
  return !_shutdown.load(std::memory_order_acquire);
}

}  // namespace arangodb
