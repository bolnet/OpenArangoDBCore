#include <benchmark/benchmark.h>

#include "Enterprise/RocksDBEngine/KeySpacePartitioner.h"
#include "Enterprise/RocksDBEngine/ChangelogBuffer.h"
#include "Enterprise/RocksDBEngine/IndexBuilderThreadPool.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Data generators
// ---------------------------------------------------------------------------

static std::vector<std::string> generateSortedKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    // Zero-padded for lexicographic order
    char buf[32];
    snprintf(buf, sizeof(buf), "key_%010zu", i);
    keys.emplace_back(buf);
  }
  return keys;
}

// ---------------------------------------------------------------------------
// BM_KeySpacePartition: partition() at various partition counts
// ---------------------------------------------------------------------------

static void BM_KeySpacePartition(benchmark::State& state) {
  auto const numPartitions = static_cast<uint32_t>(state.range(0));

  for (auto _ : state) {
    auto ranges = arangodb::KeySpacePartitioner::partition(
        "key_0000000000", "key_9999999999", numPartitions);
    benchmark::DoNotOptimize(ranges.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_KeySpacePartition)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64);

// ---------------------------------------------------------------------------
// BM_ComputeSplitPoints: from sampled keys
// ---------------------------------------------------------------------------

static void BM_ComputeSplitPoints(benchmark::State& state) {
  auto const numSamples = static_cast<size_t>(state.range(0));
  auto const numPartitions = static_cast<uint32_t>(state.range(1));
  auto const sampledKeys = generateSortedKeys(numSamples);

  for (auto _ : state) {
    auto splits = arangodb::KeySpacePartitioner::computeSplitPoints(
        sampledKeys, numPartitions);
    benchmark::DoNotOptimize(splits.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ComputeSplitPoints)
    ->Args({1000, 4})
    ->Args({1000, 8})
    ->Args({10000, 4})
    ->Args({10000, 8})
    ->Args({10000, 16})
    ->Args({100000, 8})
    ->Args({100000, 16});

// ---------------------------------------------------------------------------
// BM_ChangelogBufferAppend: single-threaded append throughput
// ---------------------------------------------------------------------------

static void BM_ChangelogBufferAppend(benchmark::State& state) {
  // 1 GB budget so we never hit the limit during benchmarking
  arangodb::ChangelogBuffer buffer(1ULL << 30);

  size_t idx = 0;
  for (auto _ : state) {
    arangodb::ChangelogEntry entry{
        arangodb::ChangelogOpType::kInsert,
        "docKey_" + std::to_string(idx),
        "rev_" + std::to_string(idx),
        R"({"value":)" + std::to_string(idx) + "}"};
    benchmark::DoNotOptimize(buffer.append(std::move(entry)));
    ++idx;

    // Reset periodically to avoid unbounded memory growth
    if (idx % 100000 == 0) {
      state.PauseTiming();
      buffer.clear();
      state.ResumeTiming();
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ChangelogBufferAppend);

// ---------------------------------------------------------------------------
// BM_ChangelogBufferAppendMultiThreaded: contended append throughput
// ---------------------------------------------------------------------------

static void BM_ChangelogBufferAppendMultiThreaded(benchmark::State& state) {
  auto const numThreads = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    state.PauseTiming();
    arangodb::ChangelogBuffer buffer(1ULL << 30);
    size_t const opsPerThread = 10000;
    std::atomic<bool> go{false};
    state.ResumeTiming();

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (size_t t = 0; t < numThreads; ++t) {
      threads.emplace_back([&buffer, &go, t, opsPerThread]() {
        while (!go.load(std::memory_order_acquire)) {
          // spin until start signal
        }
        for (size_t i = 0; i < opsPerThread; ++i) {
          arangodb::ChangelogEntry entry{
              arangodb::ChangelogOpType::kInsert,
              "t" + std::to_string(t) + "_k" + std::to_string(i),
              "r" + std::to_string(i),
              R"({"v":)" + std::to_string(i) + "}"};
          buffer.append(std::move(entry));
        }
      });
    }

    go.store(true, std::memory_order_release);
    for (auto& th : threads) {
      th.join();
    }
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(state.range(0)) * 10000);
}

BENCHMARK(BM_ChangelogBufferAppendMultiThreaded)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_ChangelogBufferForEach: iteration throughput
// ---------------------------------------------------------------------------

static void BM_ChangelogBufferForEach(benchmark::State& state) {
  auto const numEntries = static_cast<size_t>(state.range(0));
  arangodb::ChangelogBuffer buffer(1ULL << 30);

  for (size_t i = 0; i < numEntries; ++i) {
    buffer.append({arangodb::ChangelogOpType::kInsert,
                   "key_" + std::to_string(i),
                   "rev_" + std::to_string(i),
                   R"({"v":)" + std::to_string(i) + "}"});
  }

  for (auto _ : state) {
    size_t count = 0;
    buffer.forEach([&count](arangodb::ChangelogEntry const& e) {
      benchmark::DoNotOptimize(e.documentKey.data());
      ++count;
    });
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(numEntries));
}

BENCHMARK(BM_ChangelogBufferForEach)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_ThreadPoolSubmitLatency: task submission + future.get() latency
// ---------------------------------------------------------------------------

static void BM_ThreadPoolSubmitLatency(benchmark::State& state) {
  auto const numThreads = static_cast<uint32_t>(state.range(0));
  arangodb::IndexBuilderThreadPool pool(numThreads);

  for (auto _ : state) {
    auto futureOpt = pool.submit([]() -> bool { return true; });
    if (futureOpt.has_value()) {
      benchmark::DoNotOptimize(futureOpt->get());
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));

  pool.shutdown();
}

BENCHMARK(BM_ThreadPoolSubmitLatency)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_ThreadPoolBatchSubmit: submit many tasks, then wait for all
// ---------------------------------------------------------------------------

static void BM_ThreadPoolBatchSubmit(benchmark::State& state) {
  auto const numThreads = static_cast<uint32_t>(state.range(0));
  size_t const batchSize = 10000;

  for (auto _ : state) {
    state.PauseTiming();
    arangodb::IndexBuilderThreadPool pool(numThreads);
    state.ResumeTiming();

    std::vector<std::future<bool>> futures;
    futures.reserve(batchSize);

    for (size_t i = 0; i < batchSize; ++i) {
      auto futOpt = pool.submit([i]() -> bool {
        // Simulate light index work
        volatile size_t x = i * i;
        (void)x;
        return true;
      });
      if (futOpt.has_value()) {
        futures.push_back(std::move(*futOpt));
      }
    }

    for (auto& f : futures) {
      benchmark::DoNotOptimize(f.get());
    }

    pool.shutdown();
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(batchSize));
  state.counters["threads"] = benchmark::Counter(
      static_cast<double>(numThreads), benchmark::Counter::kDefaults);
}

BENCHMARK(BM_ThreadPoolBatchSubmit)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_ThreadPoolHeavyWork: tasks with non-trivial computation
// ---------------------------------------------------------------------------

static void BM_ThreadPoolHeavyWork(benchmark::State& state) {
  auto const numThreads = static_cast<uint32_t>(state.range(0));
  size_t const batchSize = 1000;

  for (auto _ : state) {
    state.PauseTiming();
    arangodb::IndexBuilderThreadPool pool(numThreads);
    state.ResumeTiming();

    std::vector<std::future<bool>> futures;
    futures.reserve(batchSize);

    for (size_t i = 0; i < batchSize; ++i) {
      auto futOpt = pool.submit([i]() -> bool {
        // Simulate heavier index building work (sorting a small array)
        std::vector<int> data(100);
        for (size_t j = 0; j < data.size(); ++j) {
          data[j] = static_cast<int>((i * 31 + j * 17) % 10000);
        }
        std::sort(data.begin(), data.end());
        return data.front() < data.back();
      });
      if (futOpt.has_value()) {
        futures.push_back(std::move(*futOpt));
      }
    }

    for (auto& f : futures) {
      benchmark::DoNotOptimize(f.get());
    }

    pool.shutdown();
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(batchSize));
}

BENCHMARK(BM_ThreadPoolHeavyWork)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMillisecond);
