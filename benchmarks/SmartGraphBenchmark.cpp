#include <benchmark/benchmark.h>

#include "Enterprise/Sharding/ShardingStrategyEE.h"
#include "Enterprise/VocBase/SmartGraphSchema.h"
#include "Enterprise/Cluster/SatelliteDistribution.h"
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Data generators
// ---------------------------------------------------------------------------

static std::vector<std::string> generateSmartKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    keys.push_back("prefix" + std::to_string(i % 64) + ":" +
                    "localkey" + std::to_string(i));
  }
  return keys;
}

static std::vector<std::string> generatePlainKeys(size_t count) {
  std::vector<std::string> keys;
  keys.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    keys.push_back("plainkey" + std::to_string(i));
  }
  return keys;
}

// ---------------------------------------------------------------------------
// BM_FnvHashThroughput: raw FNV-1a hash + mod via computeShardIndex
// ---------------------------------------------------------------------------

static void BM_FnvHashThroughput(benchmark::State& state) {
  auto const keys = generateSmartKeys(10000);
  auto const numShards = static_cast<uint32_t>(state.range(0));
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        arangodb::computeShardIndex(keys[idx % keys.size()], numShards));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_FnvHashThroughput)
    ->Arg(4)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256);

// ---------------------------------------------------------------------------
// BM_ExtractSmartPrefix: extracting the smart prefix from a key
// ---------------------------------------------------------------------------

static void BM_ExtractSmartPrefix(benchmark::State& state) {
  auto const keys = generateSmartKeys(10000);
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        arangodb::extractSmartPrefix(keys[idx % keys.size()]));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ExtractSmartPrefix);

// ---------------------------------------------------------------------------
// BM_SmartKeyValidation: SmartGraphSchema::validateDocument throughput
// ---------------------------------------------------------------------------

static void BM_SmartKeyValidation(benchmark::State& state) {
  // Valid keys: prefix matches smartAttributeValue
  auto const keys = generateSmartKeys(10000);
  size_t idx = 0;

  for (auto _ : state) {
    auto const& key = keys[idx % keys.size()];
    auto prefix = arangodb::SmartGraphSchema::extractSmartValue(key);
    auto result = arangodb::SmartGraphSchema::validateDocument(
        key, prefix, "smartAttr");
    benchmark::DoNotOptimize(result.ok);
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SmartKeyValidation);

// ---------------------------------------------------------------------------
// BM_DisjointConstraintCheck: validateEdge with isDisjoint=true
// ---------------------------------------------------------------------------

static void BM_DisjointConstraintCheck(benchmark::State& state) {
  // Build pairs where from/to share the same smart prefix (valid disjoint)
  std::vector<std::pair<std::string, std::string>> edges;
  edges.reserve(10000);
  for (size_t i = 0; i < 10000; ++i) {
    std::string prefix = "pfx" + std::to_string(i % 32);
    edges.emplace_back(prefix + ":fromKey" + std::to_string(i),
                       prefix + ":toKey" + std::to_string(i));
  }

  size_t idx = 0;
  for (auto _ : state) {
    auto const& [fromKey, toKey] = edges[idx % edges.size()];
    auto result = arangodb::SmartGraphSchema::validateEdge(
        fromKey, toKey, /*isDisjoint=*/true);
    benchmark::DoNotOptimize(result.ok);
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_DisjointConstraintCheck);

// ---------------------------------------------------------------------------
// BM_DisjointConstraintCheckMixed: mix of valid and invalid disjoint edges
// ---------------------------------------------------------------------------

static void BM_DisjointConstraintCheckMixed(benchmark::State& state) {
  std::vector<std::pair<std::string, std::string>> edges;
  edges.reserve(10000);
  for (size_t i = 0; i < 10000; ++i) {
    std::string fromPrefix = "pfx" + std::to_string(i % 32);
    // Every other edge has mismatched prefixes (invalid disjoint)
    std::string toPrefix = (i % 2 == 0) ? fromPrefix
                                         : "other" + std::to_string(i % 16);
    edges.emplace_back(fromPrefix + ":fromKey" + std::to_string(i),
                       toPrefix + ":toKey" + std::to_string(i));
  }

  size_t idx = 0;
  for (auto _ : state) {
    auto const& [fromKey, toKey] = edges[idx % edges.size()];
    auto result = arangodb::SmartGraphSchema::validateEdge(
        fromKey, toKey, /*isDisjoint=*/true);
    benchmark::DoNotOptimize(result.ok);
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_DisjointConstraintCheckMixed);

// ---------------------------------------------------------------------------
// BM_SatelliteReplicationFactorCheck: SatelliteDistribution::isSatellite
// ---------------------------------------------------------------------------

static void BM_SatelliteReplicationFactorCheck(benchmark::State& state) {
  // Alternate between satellite (0) and non-satellite values
  std::vector<uint64_t> factors = {0, 1, 2, 3, 0, 5, 0, 7, 8, 0};
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        arangodb::SatelliteDistribution::isSatellite(
            factors[idx % factors.size()]));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SatelliteReplicationFactorCheck);

// ---------------------------------------------------------------------------
// BM_SmartShardRouting: full SmartGraphProvider shard routing path
// ---------------------------------------------------------------------------

static void BM_SmartShardRouting(benchmark::State& state) {
  std::unordered_map<std::string, std::string> mapping = {
      {"vertices", "s1"}, {"edges", "s2"}};
  arangodb::graph::SmartGraphProvider provider(mapping, 16);

  auto const keys = generateSmartKeys(10000);
  size_t idx = 0;

  for (auto _ : state) {
    auto const& key = keys[idx % keys.size()];
    benchmark::DoNotOptimize(
        provider.getResponsibleShard("vertices", key));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SmartShardRouting);

// ---------------------------------------------------------------------------
// BM_ShardRoutingComparison: smart vs plain key routing overhead
// ---------------------------------------------------------------------------

static void BM_ShardRoutingWithSmartKey(benchmark::State& state) {
  auto const keys = generateSmartKeys(10000);
  size_t idx = 0;

  for (auto _ : state) {
    auto prefix = arangodb::extractSmartPrefix(keys[idx % keys.size()]);
    benchmark::DoNotOptimize(arangodb::computeShardIndex(prefix, 16));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ShardRoutingWithSmartKey);

static void BM_ShardRoutingWithPlainKey(benchmark::State& state) {
  auto const keys = generatePlainKeys(10000);
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        arangodb::computeShardIndex(keys[idx % keys.size()], 16));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ShardRoutingWithPlainKey);

// ---------------------------------------------------------------------------
// BM_SmartEdgeShardingStrategy: enterprise hash smart edge strategy
// ---------------------------------------------------------------------------

static void BM_SmartEdgeShardingStrategy(benchmark::State& state) {
  arangodb::EnterpriseHashSmartEdgeShardingStrategy strategy;
  // Edge keys: "fromSmart:toSmart:edgeKey"
  std::vector<std::string> edgeKeys;
  edgeKeys.reserve(10000);
  for (size_t i = 0; i < 10000; ++i) {
    edgeKeys.push_back("pfx" + std::to_string(i % 32) + ":pfx" +
                        std::to_string(i % 16) + ":ek" + std::to_string(i));
  }

  auto const numShards = static_cast<uint32_t>(state.range(0));
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        strategy.getResponsibleShard(edgeKeys[idx % edgeKeys.size()],
                                     numShards));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SmartEdgeShardingStrategy)
    ->Arg(4)
    ->Arg(16)
    ->Arg(64);

// ---------------------------------------------------------------------------
// BM_SmartVertexShardingStrategy: enterprise hex smart vertex strategy
// ---------------------------------------------------------------------------

static void BM_SmartVertexShardingStrategy(benchmark::State& state) {
  arangodb::EnterpriseHexSmartVertexShardingStrategy strategy;
  auto const keys = generateSmartKeys(10000);
  auto const numShards = static_cast<uint32_t>(state.range(0));
  size_t idx = 0;

  for (auto _ : state) {
    benchmark::DoNotOptimize(
        strategy.getResponsibleShard(keys[idx % keys.size()], numShards));
    ++idx;
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_SmartVertexShardingStrategy)
    ->Arg(4)
    ->Arg(16)
    ->Arg(64);
