#pragma once
#ifndef ARANGODB_IRESEARCH_OPTIMIZE_TOP_K_H
#define ARANGODB_IRESEARCH_OPTIMIZE_TOP_K_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for AQL mock types used in optimizer rule.
// In integration mode, the real ArangoDB optimizer handles TopK pattern
// detection, so these mock-dependent declarations are not needed.
#ifndef ARANGODB_INTEGRATION_BUILD
namespace arangodb {
namespace aql {
struct SortElement;
class MockExecutionNode;
}  // namespace aql
}  // namespace arangodb
#endif

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class ScoreThresholdManager
/// @brief Tracks the top-k BM25 scores using a min-heap.
///
/// The threshold (minimum score in the heap) is the k-th best score seen.
/// Documents scoring below this threshold can be safely skipped.
///
/// Thread-safety: NOT thread-safe. Designed for single-query, single-threaded
/// execution. Do not share across threads without external synchronization.
////////////////////////////////////////////////////////////////////////////////
class ScoreThresholdManager {
 public:
  explicit ScoreThresholdManager(size_t k);

  /// Submit a candidate score. Returns true if the score was competitive
  /// (inserted into the heap), false if it was below threshold.
  bool addScore(float score);

  /// Current threshold: 0.0 if fewer than k scores seen, otherwise the
  /// k-th best (minimum in the min-heap of top-k scores).
  float threshold() const;

  /// Number of scores currently tracked (max k).
  size_t size() const;

  /// Configured capacity.
  size_t capacity() const;

  /// Clear all tracked scores, reset threshold to 0.0.
  void reset();

 private:
  size_t _k;
  /// Min-heap: smallest element at top. When full, top = k-th best = threshold.
  std::vector<float> _heap;
};

////////////////////////////////////////////////////////////////////////////////
/// @class ScoredDocIterator
/// @brief Abstract base for a scored document iterator.
///
/// Mirrors the IResearch doc_iterator + score interface for standalone use.
////////////////////////////////////////////////////////////////////////////////
class ScoredDocIterator {
 public:
  virtual ~ScoredDocIterator() = default;
  virtual bool next() = 0;
  virtual uint64_t value() const = 0;
  virtual uint64_t seek(uint64_t target) = 0;
  virtual float score() = 0;
  /// Upper bound on scores of remaining documents in this posting list.
  virtual float maxScore() = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @class WandIterator
/// @brief WAND iterator that wraps a ScoredDocIterator and skips documents
/// whose max possible score is below the current competitive threshold.
///
/// This is the core early-termination mechanism for top-k BM25 queries.
/// Correctness guarantee: uses strict less-than for threshold comparison,
/// ensuring exact top-k results (never skips potentially competitive docs).
////////////////////////////////////////////////////////////////////////////////
class WandIterator : public ScoredDocIterator {
 public:
  /// @param inner    The underlying scored document iterator to wrap.
  /// @param manager  Shared threshold manager (updated as scores arrive).
  WandIterator(std::unique_ptr<ScoredDocIterator> inner,
               std::shared_ptr<ScoreThresholdManager> manager);

  bool next() override;
  uint64_t value() const override;
  uint64_t seek(uint64_t target) override;
  float score() override;
  float maxScore() override;

  /// Number of documents skipped due to threshold pruning.
  size_t skippedCount() const;

 private:
  std::unique_ptr<ScoredDocIterator> _inner;
  std::shared_ptr<ScoreThresholdManager> _manager;
  size_t _skipped = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// Optimizer rule: pattern detection
///
/// In integration mode, the real ArangoDB optimizer handles TopK pattern
/// detection (arangod's OptimizerRules). These mock-type-dependent functions
/// are only compiled in standalone mode for testing.
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INTEGRATION_BUILD
/// Detects EnumerateViewNode -> SortNode(BM25 desc) -> LimitNode(k) patterns
/// and annotates the EnumerateViewNode with WAND parameters.
/// Does not create new node types; modifies existing nodes in-place.
///
/// @param nodes  All execution nodes in the plan (flat list for traversal).
/// @return Number of patterns found and optimized.
size_t optimizeTopKPatterns(
    std::vector<aql::MockExecutionNode*> const& nodes);

/// Check if a SortElement references a BM25 scorer function.
bool isBM25SortElement(aql::SortElement const& element);

/// Registration function called from OptimizerRulesFeature::prepare()
/// under #ifdef USE_ENTERPRISE guard.
void registerOptimizeTopKRule();
#endif  // !ARANGODB_INTEGRATION_BUILD

// Forward declaration (full definition in IResearchDataStoreEE.hpp).
struct WandExecutionContext;

/// Factory: Create a WandExecutionContext from optimizer annotations.
/// @param enabled   Whether WAND was enabled by the optimizer rule.
/// @param heapSize  The k value from the LimitNode (top-k).
/// @return A fully initialized WandExecutionContext with threshold manager.
WandExecutionContext createWandContext(bool enabled, size_t heapSize);

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH_OPTIMIZE_TOP_K_H
