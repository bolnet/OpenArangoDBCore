#include <benchmark/benchmark.h>

#include "Enterprise/Aql/MinHashFunctions.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data generators
// ---------------------------------------------------------------------------

static std::vector<std::string> generateStringSet(size_t count, size_t seed = 42) {
  std::mt19937_64 rng(seed);
  std::vector<std::string> elements;
  elements.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    // Generate varied-length strings to simulate real document tokens
    std::string s;
    size_t len = 8 + (rng() % 32);
    s.reserve(len);
    for (size_t j = 0; j < len; ++j) {
      s.push_back(static_cast<char>('a' + (rng() % 26)));
    }
    elements.push_back(std::move(s));
  }
  return elements;
}

// ---------------------------------------------------------------------------
// BM_PermutationSeedGeneration: generatePermutationSeeds at various k
// ---------------------------------------------------------------------------

static void BM_PermutationSeedGeneration(benchmark::State& state) {
  auto const k = static_cast<uint32_t>(state.range(0));

  for (auto _ : state) {
    auto seeds = arangodb::generatePermutationSeeds(k);
    benchmark::DoNotOptimize(seeds.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_PermutationSeedGeneration)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Arg(1024);

// ---------------------------------------------------------------------------
// BM_MinHashSignature: end-to-end signature generation for varying set sizes
// ---------------------------------------------------------------------------

static void BM_MinHashSignature(benchmark::State& state) {
  auto const setSize = static_cast<size_t>(state.range(0));
  auto const elements = generateStringSet(setSize);
  auto const seeds = arangodb::generatePermutationSeeds(arangodb::kDefaultMinHashK);

  for (auto _ : state) {
    arangodb::MinHashGenerator gen(seeds);
    for (auto const& elem : elements) {
      gen.addElement(elem);
    }
    auto sig = gen.finalize();
    benchmark::DoNotOptimize(sig.data());
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(setSize));
}

BENCHMARK(BM_MinHashSignature)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_MinHashSignatureVaryingK: signature generation with different k values
// ---------------------------------------------------------------------------

static void BM_MinHashSignatureVaryingK(benchmark::State& state) {
  auto const k = static_cast<uint32_t>(state.range(0));
  auto const elements = generateStringSet(1000);
  auto const seeds = arangodb::generatePermutationSeeds(k);

  for (auto _ : state) {
    arangodb::MinHashGenerator gen(seeds);
    for (auto const& elem : elements) {
      gen.addElement(elem);
    }
    auto sig = gen.finalize();
    benchmark::DoNotOptimize(sig.data());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 1000);
}

BENCHMARK(BM_MinHashSignatureVaryingK)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512);

// ---------------------------------------------------------------------------
// BM_MinHashAddElement: per-element addElement cost (amortized)
// ---------------------------------------------------------------------------

static void BM_MinHashAddElement(benchmark::State& state) {
  auto const seeds = arangodb::generatePermutationSeeds(arangodb::kDefaultMinHashK);
  auto const elements = generateStringSet(10000);
  arangodb::MinHashGenerator gen(seeds);
  size_t idx = 0;

  for (auto _ : state) {
    gen.addElement(elements[idx % elements.size()]);
    ++idx;
    if (idx % 1000 == 0) {
      gen.reset();
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_MinHashAddElement);

// ---------------------------------------------------------------------------
// BM_MinHashMatchComparison: estimateJaccard throughput (signature comparison)
// ---------------------------------------------------------------------------

static void BM_MinHashMatchComparison(benchmark::State& state) {
  auto const seeds = arangodb::generatePermutationSeeds(arangodb::kDefaultMinHashK);

  // Generate two signatures from overlapping sets
  auto const setA = generateStringSet(1000, 42);
  auto const setB = generateStringSet(1000, 99);

  arangodb::MinHashGenerator genA(seeds);
  for (auto const& elem : setA) {
    genA.addElement(elem);
  }
  auto const sigA = genA.finalize();

  arangodb::MinHashGenerator genB(seeds);
  for (auto const& elem : setB) {
    genB.addElement(elem);
  }
  auto const sigB = genB.finalize();

  for (auto _ : state) {
    benchmark::DoNotOptimize(arangodb::estimateJaccard(sigA, sigB));
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_MinHashMatchComparison);

// ---------------------------------------------------------------------------
// BM_JaccardEstimationAccuracy: measure accuracy vs set overlap ratio
// This benchmark reports the estimation error as a counter, not just timing.
// ---------------------------------------------------------------------------

static void BM_JaccardEstimationAccuracy(benchmark::State& state) {
  auto const overlapPercent = static_cast<size_t>(state.range(0));
  size_t const totalSize = 1000;
  size_t const overlapSize = totalSize * overlapPercent / 100;

  auto const seeds = arangodb::generatePermutationSeeds(arangodb::kDefaultMinHashK);

  // Build two sets with controlled overlap
  std::vector<std::string> setA;
  std::vector<std::string> setB;
  setA.reserve(totalSize);
  setB.reserve(totalSize);

  // Shared elements
  for (size_t i = 0; i < overlapSize; ++i) {
    std::string shared = "shared_" + std::to_string(i);
    setA.push_back(shared);
    setB.push_back(shared);
  }
  // Unique to A
  for (size_t i = overlapSize; i < totalSize; ++i) {
    setA.push_back("onlyA_" + std::to_string(i));
  }
  // Unique to B
  for (size_t i = overlapSize; i < totalSize; ++i) {
    setB.push_back("onlyB_" + std::to_string(i));
  }

  // True Jaccard = |intersection| / |union| = overlapSize / (2*totalSize - overlapSize)
  double const trueJaccard =
      static_cast<double>(overlapSize) /
      static_cast<double>(2 * totalSize - overlapSize);

  double totalError = 0.0;
  int64_t iterations = 0;

  for (auto _ : state) {
    arangodb::MinHashGenerator genA(seeds);
    for (auto const& elem : setA) {
      genA.addElement(elem);
    }
    auto sigA = genA.finalize();

    arangodb::MinHashGenerator genB(seeds);
    for (auto const& elem : setB) {
      genB.addElement(elem);
    }
    auto sigB = genB.finalize();

    double estimated = arangodb::estimateJaccard(sigA, sigB);
    double error = std::abs(estimated - trueJaccard);
    totalError += error;
    ++iterations;
    benchmark::DoNotOptimize(estimated);
  }

  // Report average absolute error as a custom counter
  if (iterations > 0) {
    state.counters["avg_abs_error"] = benchmark::Counter(
        totalError / static_cast<double>(iterations),
        benchmark::Counter::kDefaults);
    state.counters["true_jaccard"] = benchmark::Counter(
        trueJaccard, benchmark::Counter::kDefaults);
  }
}

BENCHMARK(BM_JaccardEstimationAccuracy)
    ->Arg(10)   // 10% overlap
    ->Arg(25)   // 25% overlap
    ->Arg(50)   // 50% overlap
    ->Arg(75)   // 75% overlap
    ->Arg(90)   // 90% overlap
    ->Unit(benchmark::kMillisecond);
