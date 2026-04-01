#include <benchmark/benchmark.h>

#include "Enterprise/IResearch/IResearchOptimizeTopK.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Mock scored document iterator for benchmark use
// ---------------------------------------------------------------------------

class BenchmarkScoredDocIterator final
    : public arangodb::iresearch::ScoredDocIterator {
 public:
  BenchmarkScoredDocIterator(size_t numDocs, float maxScoreValue,
                              uint64_t seed = 42)
      : _maxScoreValue(maxScoreValue), _pos(0) {
    _docs.reserve(numDocs);
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, maxScoreValue);
    for (size_t i = 0; i < numDocs; ++i) {
      _docs.push_back({static_cast<uint64_t>(i + 1), dist(rng)});
    }
  }

  bool next() override {
    if (_pos >= _docs.size()) {
      return false;
    }
    ++_pos;
    return _pos <= _docs.size();
  }

  uint64_t value() const override {
    if (_pos == 0 || _pos > _docs.size()) {
      return 0;
    }
    return _docs[_pos - 1].docId;
  }

  uint64_t seek(uint64_t target) override {
    while (_pos < _docs.size() && _docs[_pos].docId < target) {
      ++_pos;
    }
    if (_pos < _docs.size()) {
      ++_pos;  // advance past for consistency with next()
      return _docs[_pos - 1].docId;
    }
    return 0;
  }

  float score() override {
    if (_pos == 0 || _pos > _docs.size()) {
      return 0.0f;
    }
    return _docs[_pos - 1].score;
  }

  float maxScore() override { return _maxScoreValue; }

 private:
  struct DocScore {
    uint64_t docId;
    float score;
  };

  float _maxScoreValue;
  size_t _pos;
  std::vector<DocScore> _docs;
};

// ---------------------------------------------------------------------------
// BM_ScoreThresholdInsert: addScore throughput on ScoreThresholdManager
// ---------------------------------------------------------------------------

static void BM_ScoreThresholdInsert(benchmark::State& state) {
  auto const k = static_cast<size_t>(state.range(0));
  std::mt19937_64 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 100.0f);

  // Pre-generate scores to avoid RNG overhead in the hot loop
  std::vector<float> scores(100000);
  for (auto& s : scores) {
    s = dist(rng);
  }

  size_t idx = 0;
  for (auto _ : state) {
    state.PauseTiming();
    arangodb::iresearch::ScoreThresholdManager manager(k);
    state.ResumeTiming();

    for (size_t i = 0; i < 10000; ++i) {
      benchmark::DoNotOptimize(manager.addScore(scores[idx % scores.size()]));
      ++idx;
    }
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 10000);
}

BENCHMARK(BM_ScoreThresholdInsert)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000);

// ---------------------------------------------------------------------------
// BM_ScoreThresholdCheck: threshold() query throughput after filling
// ---------------------------------------------------------------------------

static void BM_ScoreThresholdCheck(benchmark::State& state) {
  auto const k = static_cast<size_t>(state.range(0));
  arangodb::iresearch::ScoreThresholdManager manager(k);

  std::mt19937_64 rng(42);
  std::uniform_real_distribution<float> dist(0.0f, 100.0f);
  // Fill the manager so threshold is active
  for (size_t i = 0; i < k * 2; ++i) {
    manager.addScore(dist(rng));
  }

  for (auto _ : state) {
    benchmark::DoNotOptimize(manager.threshold());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ScoreThresholdCheck)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);

// ---------------------------------------------------------------------------
// BM_WandIteratorSkipRate: measure skip efficiency at various k values
// ---------------------------------------------------------------------------

static void BM_WandIteratorSkipRate(benchmark::State& state) {
  auto const k = static_cast<size_t>(state.range(0));
  size_t const numDocs = 100000;
  float const maxScore = 10.0f;

  for (auto _ : state) {
    auto manager =
        std::make_shared<arangodb::iresearch::ScoreThresholdManager>(k);

    // Pre-populate the threshold manager with some initial competitive scores
    // to trigger skipping behavior
    {
      std::mt19937_64 rng(123);
      std::uniform_real_distribution<float> dist(5.0f, maxScore);
      for (size_t i = 0; i < k; ++i) {
        manager->addScore(dist(rng));
      }
    }

    auto inner = std::make_unique<BenchmarkScoredDocIterator>(
        numDocs, maxScore, 42);
    arangodb::iresearch::WandIterator wand(std::move(inner), manager);

    size_t consumed = 0;
    while (wand.next()) {
      float s = wand.score();
      manager->addScore(s);
      benchmark::DoNotOptimize(s);
      ++consumed;
    }

    state.counters["docs_consumed"] = benchmark::Counter(
        static_cast<double>(consumed), benchmark::Counter::kDefaults);
    state.counters["docs_skipped"] = benchmark::Counter(
        static_cast<double>(wand.skippedCount()),
        benchmark::Counter::kDefaults);
    state.counters["skip_rate_pct"] = benchmark::Counter(
        static_cast<double>(wand.skippedCount()) * 100.0 /
            static_cast<double>(numDocs),
        benchmark::Counter::kDefaults);
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(numDocs));
}

BENCHMARK(BM_WandIteratorSkipRate)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(500)
    ->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_WandIteratorFullScan: baseline without threshold (no skipping)
// ---------------------------------------------------------------------------

static void BM_WandIteratorFullScan(benchmark::State& state) {
  size_t const numDocs = 100000;
  float const maxScore = 10.0f;

  for (auto _ : state) {
    // k = numDocs means threshold never prunes
    auto manager =
        std::make_shared<arangodb::iresearch::ScoreThresholdManager>(numDocs);
    auto inner = std::make_unique<BenchmarkScoredDocIterator>(
        numDocs, maxScore, 42);
    arangodb::iresearch::WandIterator wand(std::move(inner), manager);

    size_t consumed = 0;
    while (wand.next()) {
      float s = wand.score();
      manager->addScore(s);
      benchmark::DoNotOptimize(s);
      ++consumed;
    }

    state.counters["docs_consumed"] = benchmark::Counter(
        static_cast<double>(consumed), benchmark::Counter::kDefaults);
  }
  state.SetItemsProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(numDocs));
}

BENCHMARK(BM_WandIteratorFullScan)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// BM_OptimizerPatternDetection: optimizeTopKPatterns speed
// Uses the real mock execution node hierarchy from AqlMocks.h:
//   MockEnumerateViewNode -> MockSortNode(BM25 desc) -> MockLimitNode(k)
// ---------------------------------------------------------------------------

#include "AqlMocks.h"

struct PatternPlan {
  std::vector<std::unique_ptr<arangodb::aql::MockExecutionNode>> owned;
  std::vector<arangodb::aql::MockExecutionNode*> flat;
};

static PatternPlan buildOptimizerPlan(size_t numPatterns) {
  PatternPlan plan;
  plan.owned.reserve(numPatterns * 3);
  plan.flat.reserve(numPatterns * 3);

  for (size_t i = 0; i < numPatterns; ++i) {
    auto view = std::make_unique<arangodb::aql::MockEnumerateViewNode>();
    auto sort = std::make_unique<arangodb::aql::MockSortNode>();
    sort->addSortElement("BM25(doc)", /*ascending=*/false);
    auto limit = std::make_unique<arangodb::aql::MockLimitNode>();
    limit->setLimit(10);
    limit->setOffset(0);

    // Wire dependency chain: limit -> sort -> view
    sort->addDependency(view.get());
    limit->addDependency(sort.get());

    plan.flat.push_back(view.get());
    plan.flat.push_back(sort.get());
    plan.flat.push_back(limit.get());

    plan.owned.push_back(std::move(view));
    plan.owned.push_back(std::move(sort));
    plan.owned.push_back(std::move(limit));
  }
  return plan;
}

static void BM_OptimizerPatternDetection(benchmark::State& state) {
  auto const numPatterns = static_cast<size_t>(state.range(0));
  auto plan = buildOptimizerPlan(numPatterns);

  for (auto _ : state) {
    // Reset WAND annotations before each iteration
    for (auto* node : plan.flat) {
      if (node->getType() ==
          arangodb::aql::ExecutionNodeType::ENUMERATE_IRESEARCH_VIEW) {
        static_cast<arangodb::aql::MockEnumerateViewNode*>(node)
            ->setWandEnabled(false);
      }
    }

    auto count = arangodb::iresearch::optimizeTopKPatterns(plan.flat);
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_OptimizerPatternDetection)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000);
