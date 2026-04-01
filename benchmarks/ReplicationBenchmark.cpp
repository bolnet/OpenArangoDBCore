#include <benchmark/benchmark.h>

#include "Enterprise/Replication/SequenceNumberGenerator.h"
#include "Enterprise/Replication/SequenceNumberTracker.h"
#include "Enterprise/Replication/IdempotencyChecker.h"
#include "Enterprise/Replication/MessageBatcher.h"
#include "Enterprise/Replication/DirectMQProtocol.h"
#include "Enterprise/Replication/DirectMQMessage.h"
#include "Enterprise/Replication/IWALIterator.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// BM_SequenceNumberGeneratorSingle: uncontended nextSequence throughput
// ---------------------------------------------------------------------------

static void BM_SequenceNumberGeneratorSingle(benchmark::State& state) {
  arangodb::SequenceNumberGenerator gen;
  std::string const shard = "shard_0001";

  for (auto _ : state) {
    benchmark::DoNotOptimize(gen.nextSequence(shard));
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SequenceNumberGeneratorSingle);

// ---------------------------------------------------------------------------
// BM_SequenceNumberGeneratorContended: multi-threaded contention
// ---------------------------------------------------------------------------

static void BM_SequenceNumberGeneratorContended(benchmark::State& state) {
  auto const numThreads = static_cast<size_t>(state.range(0));
  size_t const opsPerThread = 100000;

  for (auto _ : state) {
    state.PauseTiming();
    arangodb::SequenceNumberGenerator gen;
    std::string const shard = "shard_contended";
    std::atomic<bool> go{false};
    state.ResumeTiming();

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (size_t t = 0; t < numThreads; ++t) {
      threads.emplace_back([&gen, &shard, &go, opsPerThread]() {
        while (!go.load(std::memory_order_acquire)) {
          // spin
        }
        for (size_t i = 0; i < opsPerThread; ++i) {
          benchmark::DoNotOptimize(gen.nextSequence(shard));
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
      static_cast<int64_t>(numThreads) *
      static_cast<int64_t>(opsPerThread));
}

BENCHMARK(BM_SequenceNumberGeneratorContended)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_SequenceNumberGeneratorMultiShard: spread across many shards
// ---------------------------------------------------------------------------

static void BM_SequenceNumberGeneratorMultiShard(benchmark::State& state) {
  auto const numShards = static_cast<size_t>(state.range(0));
  arangodb::SequenceNumberGenerator gen;

  std::vector<std::string> shards;
  shards.reserve(numShards);
  for (size_t i = 0; i < numShards; ++i) {
    shards.push_back("shard_" + std::to_string(i));
  }

  size_t idx = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        gen.nextSequence(shards[idx % numShards]));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SequenceNumberGeneratorMultiShard)
    ->Arg(1)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);

// ---------------------------------------------------------------------------
// BM_IdempotencyCheckerAccept: accept new messages throughput
// ---------------------------------------------------------------------------

static void BM_IdempotencyCheckerAccept(benchmark::State& state) {
  arangodb::SequenceNumberTracker tracker;
  arangodb::IdempotencyChecker checker(tracker);
  std::string const shard = "shard_accept";
  uint64_t seq = 1;

  for (auto _ : state) {
    arangodb::DirectMQMessage msg(shard, seq, arangodb::Operation::Insert, {});
    auto result = checker.check(msg);
    benchmark::DoNotOptimize(result);
    checker.accept(msg);
    ++seq;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_IdempotencyCheckerAccept);

// ---------------------------------------------------------------------------
// BM_IdempotencyCheckerReject: reject duplicate messages throughput
// ---------------------------------------------------------------------------

static void BM_IdempotencyCheckerReject(benchmark::State& state) {
  arangodb::SequenceNumberTracker tracker;
  arangodb::IdempotencyChecker checker(tracker);
  std::string const shard = "shard_reject";

  // Pre-apply sequence 1..1000
  for (uint64_t s = 1; s <= 1000; ++s) {
    arangodb::DirectMQMessage msg(shard, s, arangodb::Operation::Insert, {});
    checker.accept(msg);
  }

  // Now benchmark rejecting duplicates
  size_t idx = 0;
  for (auto _ : state) {
    uint64_t dupSeq = (idx % 1000) + 1;
    arangodb::DirectMQMessage msg(shard, dupSeq, arangodb::Operation::Insert,
                                  {});
    auto result = checker.check(msg);
    benchmark::DoNotOptimize(result);
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_IdempotencyCheckerReject);

// ---------------------------------------------------------------------------
// BM_IdempotencyCheckerMixed: interleaved accept/reject pattern
// ---------------------------------------------------------------------------

static void BM_IdempotencyCheckerMixed(benchmark::State& state) {
  arangodb::SequenceNumberTracker tracker;
  arangodb::IdempotencyChecker checker(tracker);
  std::string const shard = "shard_mixed";
  uint64_t nextSeq = 1;

  for (auto _ : state) {
    // Alternate: new message, then replay of previous
    arangodb::DirectMQMessage newMsg(shard, nextSeq,
                                     arangodb::Operation::Insert, {});
    auto r1 = checker.check(newMsg);
    benchmark::DoNotOptimize(r1);
    checker.accept(newMsg);

    if (nextSeq > 1) {
      arangodb::DirectMQMessage dupMsg(shard, nextSeq - 1,
                                       arangodb::Operation::Insert, {});
      auto r2 = checker.check(dupMsg);
      benchmark::DoNotOptimize(r2);
    }
    ++nextSeq;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 2);
}

BENCHMARK(BM_IdempotencyCheckerMixed);

// ---------------------------------------------------------------------------
// BM_MessageBatcherAdd: batching throughput (entries added until batch seals)
// ---------------------------------------------------------------------------

static void BM_MessageBatcherAdd(benchmark::State& state) {
  auto const batchSize = static_cast<uint32_t>(state.range(0));
  uint64_t seqCounter = 0;

  arangodb::MessageBatcher batcher(
      "shard_batcher", batchSize,
      [&seqCounter]() -> uint64_t { return ++seqCounter; });

  size_t idx = 0;
  for (auto _ : state) {
    arangodb::WALEntry entry{
        idx,                                // sequenceNumber
        idx * 1000,                         // timestamp
        "collection",                       // collectionName
        "key_" + std::to_string(idx),       // documentKey
        arangodb::WALEntry::Operation::kInsert,
        R"({"v":)" + std::to_string(idx) + "}"  // payload
    };
    auto batch = batcher.add(std::move(entry));
    if (batch.has_value()) {
      benchmark::DoNotOptimize(batch->entries.data());
    }
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_MessageBatcherAdd)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(5000);

// ---------------------------------------------------------------------------
// BM_MessageBatcherFlush: flush partial batches
// ---------------------------------------------------------------------------

static void BM_MessageBatcherFlush(benchmark::State& state) {
  auto const pendingCount = static_cast<uint32_t>(state.range(0));
  uint64_t seqCounter = 0;

  for (auto _ : state) {
    state.PauseTiming();
    arangodb::MessageBatcher batcher(
        "shard_flush", pendingCount + 1,  // batchSize > pendingCount
        [&seqCounter]() -> uint64_t { return ++seqCounter; });

    for (uint32_t i = 0; i < pendingCount; ++i) {
      arangodb::WALEntry entry{
          i, i * 1000, "col", "key_" + std::to_string(i),
          arangodb::WALEntry::Operation::kInsert, "{}"};
      batcher.add(std::move(entry));
    }
    state.ResumeTiming();

    auto batch = batcher.flush();
    if (batch.has_value()) {
      benchmark::DoNotOptimize(batch->entries.data());
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_MessageBatcherFlush)
    ->Arg(10)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000);

// ---------------------------------------------------------------------------
// BM_DirectMQFrameMessage: framing throughput at various payload sizes
// ---------------------------------------------------------------------------

static void BM_DirectMQFrameMessage(benchmark::State& state) {
  auto const payloadSize = static_cast<size_t>(state.range(0));
  std::string payload(payloadSize, 'X');

  for (auto _ : state) {
    auto frame = arangodb::DirectMQProtocol::frameMessage(payload);
    benchmark::DoNotOptimize(frame.data());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(payloadSize));
}

BENCHMARK(BM_DirectMQFrameMessage)
    ->Arg(64)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->Arg(1048576);

// ---------------------------------------------------------------------------
// BM_DirectMQParseFrame: parsing throughput at various payload sizes
// ---------------------------------------------------------------------------

static void BM_DirectMQParseFrame(benchmark::State& state) {
  auto const payloadSize = static_cast<size_t>(state.range(0));
  std::string payload(payloadSize, 'Y');
  auto const frame = arangodb::DirectMQProtocol::frameMessage(payload);

  for (auto _ : state) {
    auto result = arangodb::DirectMQProtocol::parseFrame(frame);
    benchmark::DoNotOptimize(result.ok);
    benchmark::DoNotOptimize(result.payload.data());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(payloadSize));
}

BENCHMARK(BM_DirectMQParseFrame)
    ->Arg(64)
    ->Arg(1024)
    ->Arg(16384)
    ->Arg(65536)
    ->Arg(1048576);

// ---------------------------------------------------------------------------
// BM_DirectMQBuildAck: ACK frame construction throughput
// ---------------------------------------------------------------------------

static void BM_DirectMQBuildAck(benchmark::State& state) {
  for (auto _ : state) {
    auto ack = arangodb::DirectMQProtocol::buildAck(
        arangodb::DirectMQProtocol::kAckSuccess);
    benchmark::DoNotOptimize(ack.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_DirectMQBuildAck);

// ---------------------------------------------------------------------------
// BM_DirectMQParseAck: ACK frame parsing throughput
// ---------------------------------------------------------------------------

static void BM_DirectMQParseAck(benchmark::State& state) {
  auto const ack = arangodb::DirectMQProtocol::buildAck(
      arangodb::DirectMQProtocol::kAckSuccess);

  for (auto _ : state) {
    auto result = arangodb::DirectMQProtocol::parseAck(ack);
    benchmark::DoNotOptimize(result.ok);
    benchmark::DoNotOptimize(result.statusCode);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_DirectMQParseAck);

// ---------------------------------------------------------------------------
// BM_DirectMQFrameParseRoundtrip: full frame + parse cycle
// ---------------------------------------------------------------------------

static void BM_DirectMQFrameParseRoundtrip(benchmark::State& state) {
  auto const payloadSize = static_cast<size_t>(state.range(0));
  std::string payload(payloadSize, 'Z');

  for (auto _ : state) {
    auto frame = arangodb::DirectMQProtocol::frameMessage(payload);
    auto result = arangodb::DirectMQProtocol::parseFrame(frame);
    benchmark::DoNotOptimize(result.payload.data());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(payloadSize) * 2);  // frame + parse
}

BENCHMARK(BM_DirectMQFrameParseRoundtrip)
    ->Arg(64)
    ->Arg(1024)
    ->Arg(65536);
