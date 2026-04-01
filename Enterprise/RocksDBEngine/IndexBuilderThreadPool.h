#pragma once
#ifndef ARANGODB_INDEX_BUILDER_THREAD_POOL_H
#define ARANGODB_INDEX_BUILDER_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

namespace arangodb {

class IndexBuilderThreadPool {
 public:
  explicit IndexBuilderThreadPool(uint32_t numThreads);
  ~IndexBuilderThreadPool();

  // Non-copyable, non-movable
  IndexBuilderThreadPool(IndexBuilderThreadPool const&) = delete;
  IndexBuilderThreadPool& operator=(IndexBuilderThreadPool const&) = delete;

  /// Submit a task. Returns a future for the bool result.
  /// Returns std::nullopt if pool is shut down.
  std::optional<std::future<bool>> submit(std::function<bool()> task);

  /// Gracefully shut down: finish running tasks, reject new ones.
  void shutdown();

  /// Number of worker threads.
  uint32_t numThreads() const;

  /// Whether the pool is accepting new tasks.
  bool isRunning() const;

 private:
  void workerLoop();

  std::vector<std::thread> _workers;
  std::queue<std::packaged_task<bool()>> _taskQueue;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::atomic<bool> _shutdown;
  uint32_t _numThreads;
};

}  // namespace arangodb
#endif
