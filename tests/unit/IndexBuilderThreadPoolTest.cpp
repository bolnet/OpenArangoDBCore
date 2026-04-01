#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>

#include "Enterprise/RocksDBEngine/IndexBuilderThreadPool.h"

using arangodb::IndexBuilderThreadPool;

// --- Create_DefaultThreads_HasFourWorkers ---
TEST(IndexBuilderThreadPoolTest, Create_DefaultThreads_HasFourWorkers) {
  IndexBuilderThreadPool pool(4);
  EXPECT_EQ(4u, pool.numThreads());
  EXPECT_TRUE(pool.isRunning());
}

// --- Create_CustomThreadCount_Respected ---
TEST(IndexBuilderThreadPoolTest, Create_CustomThreadCount_Respected) {
  IndexBuilderThreadPool pool(8);
  EXPECT_EQ(8u, pool.numThreads());
  EXPECT_TRUE(pool.isRunning());
}

// --- Submit_Task_Executes ---
TEST(IndexBuilderThreadPoolTest, Submit_Task_Executes) {
  IndexBuilderThreadPool pool(2);

  std::atomic<bool> executed{false};
  auto result = pool.submit([&executed]() -> bool {
    executed.store(true);
    return true;
  });

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->get());
  EXPECT_TRUE(executed.load());
}

// --- Submit_MultipleTasks_AllComplete ---
TEST(IndexBuilderThreadPoolTest, Submit_MultipleTasks_AllComplete) {
  IndexBuilderThreadPool pool(4);

  constexpr int kTasks = 10;
  std::atomic<int> completedCount{0};
  std::vector<std::future<bool>> futures;

  for (int i = 0; i < kTasks; ++i) {
    auto result = pool.submit([&completedCount]() -> bool {
      completedCount.fetch_add(1);
      return true;
    });
    ASSERT_TRUE(result.has_value());
    futures.push_back(std::move(result.value()));
  }

  for (auto& f : futures) {
    EXPECT_TRUE(f.get());
  }

  EXPECT_EQ(kTasks, completedCount.load());
}

// --- Shutdown_WaitsForRunningTasks ---
TEST(IndexBuilderThreadPoolTest, Shutdown_WaitsForRunningTasks) {
  IndexBuilderThreadPool pool(2);

  std::atomic<bool> taskFinished{false};

  auto result = pool.submit([&taskFinished]() -> bool {
    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    taskFinished.store(true);
    return true;
  });

  ASSERT_TRUE(result.has_value());

  pool.shutdown();

  // After shutdown returns, the task must have finished
  EXPECT_TRUE(taskFinished.load());
  EXPECT_FALSE(pool.isRunning());
}

// --- Shutdown_RejectsNewTasks ---
TEST(IndexBuilderThreadPoolTest, Shutdown_RejectsNewTasks) {
  IndexBuilderThreadPool pool(2);
  pool.shutdown();

  auto result = pool.submit([]() -> bool { return true; });
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(pool.isRunning());
}
