#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "AqlMocks.h"
#include "Enterprise/IResearch/IResearchDataStoreEE.hpp"
#include "Enterprise/IResearch/IResearchOptimizeTopK.h"

using namespace arangodb::iresearch;
using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
// Helper: Mock ScoredDocIterator backed by a vector of (docId, score, maxScore)
////////////////////////////////////////////////////////////////////////////////

struct DocEntry {
  uint64_t docId;
  float docScore;
  float docMaxScore;
};

class MockScoredDocIterator : public ScoredDocIterator {
 public:
  explicit MockScoredDocIterator(std::vector<DocEntry> docs)
      : _docs(std::move(docs)) {}

  bool next() override {
    ++_pos;
    return _pos < static_cast<int64_t>(_docs.size());
  }

  uint64_t value() const override {
    if (_pos < 0 || _pos >= static_cast<int64_t>(_docs.size())) {
      return 0;
    }
    return _docs[static_cast<size_t>(_pos)].docId;
  }

  uint64_t seek(uint64_t target) override {
    while (_pos + 1 < static_cast<int64_t>(_docs.size())) {
      ++_pos;
      if (_docs[static_cast<size_t>(_pos)].docId >= target) {
        return _docs[static_cast<size_t>(_pos)].docId;
      }
    }
    // If we haven't found the target yet but there might be one more
    if (_pos + 1 == static_cast<int64_t>(_docs.size())) {
      ++_pos;
      if (_docs[static_cast<size_t>(_pos)].docId >= target) {
        return _docs[static_cast<size_t>(_pos)].docId;
      }
    }
    return 0;
  }

  float score() override {
    if (_pos < 0 || _pos >= static_cast<int64_t>(_docs.size())) {
      return 0.0f;
    }
    return _docs[static_cast<size_t>(_pos)].docScore;
  }

  float maxScore() override {
    if (_pos < 0 || _pos >= static_cast<int64_t>(_docs.size())) {
      return 0.0f;
    }
    return _docs[static_cast<size_t>(_pos)].docMaxScore;
  }

 private:
  std::vector<DocEntry> _docs;
  int64_t _pos = -1;
};

////////////////////////////////////////////////////////////////////////////////
// ScoreThresholdManager tests (7 tests) — TOPK-01
////////////////////////////////////////////////////////////////////////////////

TEST(IResearchOptimizeTopK, ScoreThresholdManager_Empty_ThresholdIsZero) {
  ScoreThresholdManager manager(5);
  EXPECT_FLOAT_EQ(manager.threshold(), 0.0f);
  EXPECT_EQ(manager.size(), 0u);
  EXPECT_EQ(manager.capacity(), 5u);
}

TEST(IResearchOptimizeTopK,
     ScoreThresholdManager_BelowCapacity_ThresholdStaysZero) {
  ScoreThresholdManager manager(5);
  manager.addScore(1.0f);
  manager.addScore(2.0f);
  manager.addScore(3.0f);
  // Only 3 of 5 capacity filled
  EXPECT_FLOAT_EQ(manager.threshold(), 0.0f);
  EXPECT_EQ(manager.size(), 3u);
}

TEST(IResearchOptimizeTopK,
     ScoreThresholdManager_AtCapacity_ThresholdIsKthBest) {
  ScoreThresholdManager manager(3);
  manager.addScore(5.0f);
  manager.addScore(3.0f);
  manager.addScore(8.0f);
  // Heap has [3.0, 5.0, 8.0], threshold = min = 3.0
  EXPECT_FLOAT_EQ(manager.threshold(), 3.0f);
  EXPECT_EQ(manager.size(), 3u);
}

TEST(IResearchOptimizeTopK,
     ScoreThresholdManager_ExceedCapacity_EvictsLowest) {
  ScoreThresholdManager manager(3);
  manager.addScore(5.0f);
  manager.addScore(3.0f);
  manager.addScore(8.0f);
  // Threshold is 3.0. Add 10.0 which should evict 3.0.
  bool inserted = manager.addScore(10.0f);
  EXPECT_TRUE(inserted);
  // New heap: [5.0, 8.0, 10.0], threshold = 5.0
  EXPECT_FLOAT_EQ(manager.threshold(), 5.0f);
  EXPECT_EQ(manager.size(), 3u);
}

TEST(IResearchOptimizeTopK,
     ScoreThresholdManager_ExceedCapacity_IgnoresLower) {
  ScoreThresholdManager manager(3);
  manager.addScore(5.0f);
  manager.addScore(3.0f);
  manager.addScore(8.0f);
  // Threshold is 3.0. Adding 2.0 should be rejected.
  bool inserted = manager.addScore(2.0f);
  EXPECT_FALSE(inserted);
  EXPECT_FLOAT_EQ(manager.threshold(), 3.0f);
  EXPECT_EQ(manager.size(), 3u);
}

TEST(IResearchOptimizeTopK,
     ScoreThresholdManager_K1_ThresholdUpdatesEveryBetterScore) {
  ScoreThresholdManager manager(1);
  manager.addScore(2.0f);
  EXPECT_FLOAT_EQ(manager.threshold(), 2.0f);

  manager.addScore(5.0f);
  EXPECT_FLOAT_EQ(manager.threshold(), 5.0f);

  // Lower score should not change threshold
  manager.addScore(3.0f);
  EXPECT_FLOAT_EQ(manager.threshold(), 5.0f);

  manager.addScore(9.0f);
  EXPECT_FLOAT_EQ(manager.threshold(), 9.0f);
}

TEST(IResearchOptimizeTopK, ScoreThresholdManager_Reset_ClearsState) {
  ScoreThresholdManager manager(3);
  manager.addScore(5.0f);
  manager.addScore(3.0f);
  manager.addScore(8.0f);
  EXPECT_FLOAT_EQ(manager.threshold(), 3.0f);

  manager.reset();
  EXPECT_FLOAT_EQ(manager.threshold(), 0.0f);
  EXPECT_EQ(manager.size(), 0u);
  EXPECT_EQ(manager.capacity(), 3u);
}

////////////////////////////////////////////////////////////////////////////////
// WandIterator tests (6 tests) — TOPK-02
////////////////////////////////////////////////////////////////////////////////

TEST(IResearchOptimizeTopK, WandIterator_NoThreshold_ReturnsAllDocs) {
  // Threshold stays 0 (manager capacity = 100, only 3 docs).
  // All documents should pass through.
  std::vector<DocEntry> docs = {
      {1, 1.0f, 5.0f}, {2, 2.0f, 5.0f}, {3, 3.0f, 5.0f}};
  auto inner = std::make_unique<MockScoredDocIterator>(docs);
  auto manager = std::make_shared<ScoreThresholdManager>(100);

  WandIterator wand(std::move(inner), manager);

  std::vector<uint64_t> result;
  while (wand.next()) {
    wand.score();  // trigger threshold update
    result.push_back(wand.value());
  }
  EXPECT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], 1u);
  EXPECT_EQ(result[1], 2u);
  EXPECT_EQ(result[2], 3u);
  EXPECT_EQ(wand.skippedCount(), 0u);
}

TEST(IResearchOptimizeTopK, WandIterator_WithThreshold_SkipsBelowThreshold) {
  // Pre-fill the manager so threshold is already high.
  auto manager = std::make_shared<ScoreThresholdManager>(2);
  manager->addScore(8.0f);
  manager->addScore(9.0f);
  // Threshold is 8.0. Only docs with maxScore >= 8.0 pass.

  std::vector<DocEntry> docs = {
      {1, 1.0f, 2.0f},   // maxScore 2.0 < 8.0 -> skip
      {2, 2.0f, 3.0f},   // maxScore 3.0 < 8.0 -> skip
      {3, 9.0f, 10.0f},  // maxScore 10.0 >= 8.0 -> pass
      {4, 1.0f, 1.0f},   // maxScore 1.0 < 8.0 -> skip (threshold may rise)
  };
  auto inner = std::make_unique<MockScoredDocIterator>(docs);

  WandIterator wand(std::move(inner), manager);

  std::vector<uint64_t> result;
  while (wand.next()) {
    wand.score();
    result.push_back(wand.value());
  }
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], 3u);
  EXPECT_GE(wand.skippedCount(), 2u);  // at least docs 1 and 2 skipped
}

TEST(IResearchOptimizeTopK,
     WandIterator_ThresholdUpdates_AffectsFutureSkips) {
  // Start with empty manager (k=2). First two docs fill it, then threshold
  // rises, and subsequent low-scoring docs get skipped.
  auto manager = std::make_shared<ScoreThresholdManager>(2);

  std::vector<DocEntry> docs = {
      {1, 8.0f, 10.0f},  // pass (threshold 0)
      {2, 9.0f, 10.0f},  // pass (threshold 0), fills heap -> threshold=8.0
      {3, 1.0f, 2.0f},   // maxScore 2.0 < 8.0 -> skip
      {4, 1.0f, 3.0f},   // maxScore 3.0 < 8.0 -> skip
      {5, 10.0f, 11.0f},  // maxScore 11.0 >= 8.0 -> pass
  };
  auto inner = std::make_unique<MockScoredDocIterator>(docs);

  WandIterator wand(std::move(inner), manager);

  std::vector<uint64_t> result;
  while (wand.next()) {
    wand.score();
    result.push_back(wand.value());
  }
  EXPECT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], 1u);
  EXPECT_EQ(result[1], 2u);
  EXPECT_EQ(result[2], 5u);
  EXPECT_EQ(wand.skippedCount(), 2u);
}

TEST(IResearchOptimizeTopK, WandIterator_Seek_RespectsThreshold) {
  auto manager = std::make_shared<ScoreThresholdManager>(1);
  manager->addScore(7.0f);
  // Threshold = 7.0

  std::vector<DocEntry> docs = {
      {10, 1.0f, 2.0f},   // maxScore < 7.0
      {20, 3.0f, 5.0f},   // maxScore < 7.0
      {30, 8.0f, 10.0f},  // maxScore >= 7.0 -> competitive
  };
  auto inner = std::make_unique<MockScoredDocIterator>(docs);

  WandIterator wand(std::move(inner), manager);

  // Seek to doc 10 -- doc 10 has maxScore 2.0 < threshold 7.0,
  // so it should advance past non-competitive to doc 30.
  uint64_t result = wand.seek(10);
  EXPECT_EQ(result, 30u);
}

TEST(IResearchOptimizeTopK, WandIterator_EmptyInner_ReturnsNoDoc) {
  std::vector<DocEntry> docs;  // empty
  auto inner = std::make_unique<MockScoredDocIterator>(docs);
  auto manager = std::make_shared<ScoreThresholdManager>(5);

  WandIterator wand(std::move(inner), manager);

  EXPECT_FALSE(wand.next());
  EXPECT_EQ(wand.skippedCount(), 0u);
}

TEST(IResearchOptimizeTopK, WandIterator_ExactTopK_MatchesBruteForce) {
  // Create a dataset of 20 documents with known scores.
  // Verify that WAND top-3 matches brute-force top-3.
  std::vector<DocEntry> docs;
  // Scores: 1, 2, 3, ..., 20. maxScore = score for simplicity.
  for (uint64_t i = 1; i <= 20; ++i) {
    float s = static_cast<float>(i);
    docs.push_back({i, s, s});
  }

  // Brute force: sort all scores desc, take top 3 => {20, 19, 18}
  std::vector<float> allScores;
  for (auto const& d : docs) {
    allScores.push_back(d.docScore);
  }
  std::sort(allScores.begin(), allScores.end(), std::greater<float>{});
  std::vector<float> bruteForceTop3(allScores.begin(), allScores.begin() + 3);

  // WAND approach
  auto manager = std::make_shared<ScoreThresholdManager>(3);
  auto inner = std::make_unique<MockScoredDocIterator>(docs);
  WandIterator wand(std::move(inner), manager);

  std::vector<float> wandScores;
  while (wand.next()) {
    float s = wand.score();
    wandScores.push_back(s);
  }

  // The WAND iterator should have returned some documents (potentially all
  // that were competitive). Collect the top-3 from wandScores.
  std::sort(wandScores.begin(), wandScores.end(), std::greater<float>{});
  std::vector<float> wandTop3;
  for (size_t i = 0; i < 3 && i < wandScores.size(); ++i) {
    wandTop3.push_back(wandScores[i]);
  }

  EXPECT_EQ(wandTop3.size(), bruteForceTop3.size());
  for (size_t i = 0; i < bruteForceTop3.size(); ++i) {
    EXPECT_FLOAT_EQ(wandTop3[i], bruteForceTop3[i]);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Optimizer Rule Pattern Detection tests (7 tests) — TOPK-03
////////////////////////////////////////////////////////////////////////////////

/// Helper to build the standard pattern:
///   EnumerateViewNode <- SortNode <- LimitNode
/// (dependency chain: Limit depends on Sort depends on View)
struct PatternBuilder {
  MockEnumerateViewNode viewNode;
  MockSortNode sortNode;
  MockLimitNode limitNode;

  PatternBuilder() {
    // Chain: LimitNode -> SortNode -> EnumerateViewNode
    sortNode.addDependency(&viewNode);
    limitNode.addDependency(&sortNode);
  }

  std::vector<MockExecutionNode*> allNodes() {
    return {&viewNode, &sortNode, &limitNode};
  }
};

TEST(IResearchOptimizeTopK, OptimizerRule_CorrectPattern_EnablesWand) {
  PatternBuilder pb;
  pb.sortNode.addSortElement("BM25(doc)", false);  // BM25 descending
  pb.limitNode.setLimit(10);
  pb.limitNode.setOffset(0);

  auto nodes = pb.allNodes();
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 1u);
  EXPECT_TRUE(pb.viewNode.wandEnabled());
  EXPECT_EQ(pb.viewNode.wandHeapSize(), 10u);
}

TEST(IResearchOptimizeTopK, OptimizerRule_NoBM25Sort_NoChange) {
  PatternBuilder pb;
  pb.sortNode.addSortElement("timestamp", false);  // not BM25
  pb.limitNode.setLimit(10);
  pb.limitNode.setOffset(0);

  auto nodes = pb.allNodes();
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 0u);
  EXPECT_FALSE(pb.viewNode.wandEnabled());
}

TEST(IResearchOptimizeTopK, OptimizerRule_AscendingBM25_NoChange) {
  PatternBuilder pb;
  pb.sortNode.addSortElement("BM25(doc)", true);  // ascending = wrong
  pb.limitNode.setLimit(10);
  pb.limitNode.setOffset(0);

  auto nodes = pb.allNodes();
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 0u);
  EXPECT_FALSE(pb.viewNode.wandEnabled());
}

TEST(IResearchOptimizeTopK, OptimizerRule_NoLimit_NoChange) {
  // Build only View -> Sort, no LimitNode in the list
  MockEnumerateViewNode viewNode;
  MockSortNode sortNode;
  sortNode.addSortElement("BM25(doc)", false);
  sortNode.addDependency(&viewNode);

  std::vector<MockExecutionNode*> nodes = {&viewNode, &sortNode};
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 0u);
  EXPECT_FALSE(viewNode.wandEnabled());
}

TEST(IResearchOptimizeTopK, OptimizerRule_LimitWithOffset_NoChange) {
  PatternBuilder pb;
  pb.sortNode.addSortElement("BM25(doc)", false);
  pb.limitNode.setLimit(10);
  pb.limitNode.setOffset(5);  // offset > 0, not pure top-k

  auto nodes = pb.allNodes();
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 0u);
  EXPECT_FALSE(pb.viewNode.wandEnabled());
}

TEST(IResearchOptimizeTopK, OptimizerRule_MultipleViews_OptimizesBoth) {
  // Two independent chains
  PatternBuilder pb1;
  pb1.sortNode.addSortElement("BM25(a)", false);
  pb1.limitNode.setLimit(5);
  pb1.limitNode.setOffset(0);

  PatternBuilder pb2;
  pb2.sortNode.addSortElement("bm25(b)", false);  // lowercase
  pb2.limitNode.setLimit(20);
  pb2.limitNode.setOffset(0);

  std::vector<MockExecutionNode*> nodes;
  auto n1 = pb1.allNodes();
  auto n2 = pb2.allNodes();
  nodes.insert(nodes.end(), n1.begin(), n1.end());
  nodes.insert(nodes.end(), n2.begin(), n2.end());

  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 2u);
  EXPECT_TRUE(pb1.viewNode.wandEnabled());
  EXPECT_EQ(pb1.viewNode.wandHeapSize(), 5u);
  EXPECT_TRUE(pb2.viewNode.wandEnabled());
  EXPECT_EQ(pb2.viewNode.wandHeapSize(), 20u);
}

TEST(IResearchOptimizeTopK,
     OptimizerRule_FilterBetweenSortAndView_NoChange) {
  // Chain: View <- Filter <- Sort <- Limit
  // The Filter between Sort and View breaks the pattern.
  MockEnumerateViewNode viewNode;
  MockExecutionNode filterNode(ExecutionNodeType::FILTER);
  MockSortNode sortNode;
  MockLimitNode limitNode;

  filterNode.addDependency(&viewNode);
  sortNode.addDependency(&filterNode);
  limitNode.addDependency(&sortNode);
  sortNode.addSortElement("BM25(doc)", false);
  limitNode.setLimit(10);
  limitNode.setOffset(0);

  std::vector<MockExecutionNode*> nodes = {&viewNode, &filterNode, &sortNode,
                                           &limitNode};
  size_t count = optimizeTopKPatterns(nodes);

  EXPECT_EQ(count, 0u);
  EXPECT_FALSE(viewNode.wandEnabled());
}
