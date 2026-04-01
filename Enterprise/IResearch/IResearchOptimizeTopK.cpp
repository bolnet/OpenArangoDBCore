////////////////////////////////////////////////////////////////////////////////
/// @file IResearchOptimizeTopK.cpp
/// @brief TopK/WAND optimizer rule for IResearch views.
///
/// This file implements the WAND (Weak AND) optimization for top-k BM25
/// queries over IResearch views in ArangoDB. The optimization works in two
/// phases:
///
/// Phase 1 - Plan-time pattern detection (optimizeTopKPatterns):
///   The AQL optimizer walks the execution plan looking for the pattern:
///     EnumerateViewNode -> SortNode(BM25 DESC) -> LimitNode(k)
///   When found, it annotates the EnumerateViewNode with:
///     _enableWand = true
///     _wandHeapSize = k (the limit value)
///
/// Phase 2 - Execution-time early termination (WandIterator):
///   The WandIterator wraps the IResearch posting list iterator and maintains
///   a ScoreThresholdManager (min-heap of size k). As documents are scored,
///   the threshold (k-th best score) rises. Documents whose maxScore() is
///   strictly below the threshold are skipped without full scoring.
///
/// Correctness guarantee:
///   WAND returns EXACT top-k results. The threshold comparison uses strict
///   less-than (<), so documents with maxScore equal to the threshold are
///   never skipped. This means any document that COULD be in the top-k will
///   be fully scored.
///
/// Performance:
///   For a collection of N documents with top-k query (k << N), WAND
///   typically scores O(k * log(k)) to O(k * sqrt(N)) documents instead
///   of all N, depending on score distribution. The min-heap operations
///   are O(log k) per scored document.
///
/// Thread safety:
///   ScoreThresholdManager and WandIterator are NOT thread-safe. They are
///   designed for single-query, single-threaded execution within the AQL
///   engine. Do not share instances across threads without external
///   synchronization.
////////////////////////////////////////////////////////////////////////////////

#include "IResearchOptimizeTopK.h"

#include "IResearchDataStoreEE.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// AQL type compatibility — mock types in standalone, real types in integration
#include "Enterprise/IResearch/AqlCompat.h"

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// ScoreThresholdManager implementation
///
/// The manager uses a min-heap (std::greater comparator) to track the top-k
/// scores. The heap invariant ensures that _heap.front() is always the
/// smallest element among the k largest scores seen so far.
///
/// When the heap has fewer than k elements, the threshold is 0.0f (accept
/// all documents). Once full, the threshold equals _heap.front().
///
/// The addScore operation is O(log k):
///   - If heap is not full: push_heap O(log k)
///   - If score > threshold: pop_heap + push_heap O(log k)
///   - If score <= threshold: O(1) rejection
////////////////////////////////////////////////////////////////////////////////

ScoreThresholdManager::ScoreThresholdManager(size_t k) : _k(k) {
  _heap.reserve(k);
}

bool ScoreThresholdManager::addScore(float score) {
  if (_heap.size() < _k) {
    _heap.push_back(score);
    std::push_heap(_heap.begin(), _heap.end(), std::greater<float>{});
    return true;
  }
  // Heap is full. Only insert if score strictly exceeds current threshold.
  if (score > _heap.front()) {
    std::pop_heap(_heap.begin(), _heap.end(), std::greater<float>{});
    _heap.back() = score;
    std::push_heap(_heap.begin(), _heap.end(), std::greater<float>{});
    return true;
  }
  return false;
}

float ScoreThresholdManager::threshold() const {
  if (_heap.size() < _k) {
    return 0.0f;
  }
  return _heap.front();
}

size_t ScoreThresholdManager::size() const { return _heap.size(); }

size_t ScoreThresholdManager::capacity() const { return _k; }

void ScoreThresholdManager::reset() { _heap.clear(); }

////////////////////////////////////////////////////////////////////////////////
/// WandIterator implementation
///
/// The iterator wraps a ScoredDocIterator and applies the WAND early
/// termination strategy. On each call to next(), it checks the wrapped
/// iterator's maxScore() against the current threshold from the
/// ScoreThresholdManager. If maxScore() is strictly less than the threshold,
/// the document is skipped (not scored). Otherwise, the document is
/// returned to the caller for full scoring.
///
/// The score() method not only returns the document's actual score but also
/// feeds it into the ScoreThresholdManager, which may raise the threshold
/// for subsequent documents. This creates a positive feedback loop:
///   1. Early documents fill the heap (threshold stays 0).
///   2. Once k documents are scored, the threshold starts rising.
///   3. As the threshold rises, more documents get skipped.
///   4. The best documents raise the threshold further.
///
/// The net effect is that for skewed score distributions (common in BM25),
/// the vast majority of documents are skipped after the first few hundred
/// are scored.
////////////////////////////////////////////////////////////////////////////////

WandIterator::WandIterator(std::unique_ptr<ScoredDocIterator> inner,
                           std::shared_ptr<ScoreThresholdManager> manager)
    : _inner(std::move(inner)), _manager(std::move(manager)) {}

bool WandIterator::next() {
  while (_inner->next()) {
    // Strict less-than: docs with maxScore == threshold are NOT skipped.
    // This ensures exact top-k results. The maxScore() is an upper bound
    // on the actual score -- if maxScore >= threshold, the document might
    // still score high enough to enter the top-k.
    if (_inner->maxScore() >= _manager->threshold()) {
      return true;  // potentially competitive, caller will score()
    }
    ++_skipped;
  }
  return false;
}

uint64_t WandIterator::value() const { return _inner->value(); }

uint64_t WandIterator::seek(uint64_t target) {
  auto docId = _inner->seek(target);
  // After seek, check competitiveness; if not competitive, advance
  // to the next competitive document using next().
  if (docId != 0 && _inner->maxScore() < _manager->threshold()) {
    if (!next()) {
      return 0;
    }
    return _inner->value();
  }
  return docId;
}

float WandIterator::score() {
  float s = _inner->score();
  // Feed the actual score into the threshold manager. This may raise
  // the threshold, causing future documents to be skipped.
  _manager->addScore(s);
  return s;
}

float WandIterator::maxScore() { return _inner->maxScore(); }

size_t WandIterator::skippedCount() const { return _skipped; }

////////////////////////////////////////////////////////////////////////////////
/// WandExecutionContext implementation
///
/// The execution context bridges plan-time optimizer annotations to
/// execution-time iterator wrapping. When the execution engine creates
/// an IResearch data store iterator for a view node that has WAND enabled,
/// it calls wrapIterator() to optionally wrap the base iterator with a
/// WandIterator.
///
/// If WAND is not enabled (e.g., the optimizer rule did not fire, or
/// the rule was disabled by the user), wrapIterator() returns the base
/// iterator unchanged -- zero overhead for non-WAND queries.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ScoredDocIterator> WandExecutionContext::wrapIterator(
    std::unique_ptr<ScoredDocIterator> base) const {
  if (!enabled || heapSize == 0 || !thresholdManager) {
    return base;  // pass-through, no WAND
  }
  return std::make_unique<WandIterator>(std::move(base), thresholdManager);
}

////////////////////////////////////////////////////////////////////////////////
/// Optimizer rule: BM25 sort element detection
///
/// Checks whether a SortElement references a BM25 scorer by looking for
/// the substring "bm25" (case-insensitive) in the attribute path.
///
/// In the full ArangoDB build, this would inspect the scorer function
/// reference on the SortElement. For the standalone build, we use string
/// matching on the attribute path as a reasonable proxy.
///
/// Examples that match:
///   "BM25(doc)"        -> matches
///   "bm25(doc, 1.2)"   -> matches
///   "doc.BM25_score"   -> matches (contains "bm25")
///
/// Examples that don't match:
///   "timestamp"        -> no match
///   "TFIDF(doc)"       -> no match
////////////////////////////////////////////////////////////////////////////////

bool isBM25SortElement(aql::SortElement const& element) {
  auto const& path = element.attributePath;
  std::string lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return lower.find("bm25") != std::string::npos;
}

////////////////////////////////////////////////////////////////////////////////
/// Internal helpers: Validate preconditions for WAND optimization.
///
/// These functions check whether individual nodes satisfy the preconditions
/// for the WAND optimization:
///   - validateSortNodeForWand: primary sort is BM25 descending
///   - validateLimitNodeForWand: offset==0, limit>0
////////////////////////////////////////////////////////////////////////////////

namespace {

bool validateSortNodeForWand(aql::MockSortNode const* sortNode) {
  if (sortNode == nullptr) {
    return false;
  }

  auto const& elements = sortNode->elements();
  if (elements.empty()) {
    return false;
  }

  // Only the primary sort element matters for WAND applicability.
  // Secondary sort elements (e.g., BM25 DESC, name ASC) do not affect
  // whether WAND can be applied -- they only affect tie-breaking order
  // which is handled after the top-k candidates are identified.
  if (!isBM25SortElement(elements[0])) {
    return false;
  }

  // Must be descending: top-k wants the highest BM25 scores.
  if (elements[0].ascending) {
    return false;
  }

  return true;
}

bool validateLimitNodeForWand(aql::MockLimitNode const* limitNode) {
  if (limitNode == nullptr) {
    return false;
  }

  // WAND only works for pure top-k: offset must be zero.
  // A query like LIMIT 5, 10 (skip 5, take 10) would require the
  // heap to track offset + limit = 15 documents, which adds complexity.
  // For v1, we reject any query with offset > 0.
  if (limitNode->offset() != 0) {
    return false;
  }

  // Limit of zero makes no sense for top-k.
  if (limitNode->limit() == 0) {
    return false;
  }

  return true;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
/// Optimizer rule: pattern detection
///
/// Walks the flat list of execution nodes looking for LimitNode entries.
/// For each LimitNode, checks the dependency chain:
///   LimitNode -> SortNode(BM25 desc) -> EnumerateViewNode
///
/// If the pattern matches and the LimitNode has offset==0, the
/// EnumerateViewNode is annotated with WAND parameters:
///   _enableWand = true
///   _wandHeapSize = limitNode->limit()
///
/// This annotation is later consumed at execution time by
/// WandExecutionContext::wrapIterator() to insert a WandIterator
/// around the IResearch posting list iterator.
///
/// The pattern detection is conservative: it only matches the exact
/// three-node chain with no intervening nodes. A FILTER between
/// the SortNode and EnumerateViewNode would break the pattern and
/// prevent optimization. This is intentional -- a filter might change
/// which documents are eligible, making WAND's maxScore-based pruning
/// potentially unsafe.
////////////////////////////////////////////////////////////////////////////////

size_t optimizeTopKPatterns(
    std::vector<aql::MockExecutionNode*> const& nodes) {
  size_t optimized = 0;

  for (auto* node : nodes) {
    // Step 1: Find LimitNode as the starting point (bottom-up traversal).
    // We scan for LIMIT nodes because they are the terminal node in the
    // pattern and are less common than other node types.
    if (node->getType() != aql::ExecutionNodeType::LIMIT) {
      continue;
    }

    auto* limitNode = static_cast<aql::MockLimitNode*>(node);

    // Validate the LimitNode preconditions (offset==0, limit>0).
    if (!validateLimitNodeForWand(limitNode)) {
      continue;
    }

    // Step 2: Check that the LimitNode's dependency is a SortNode.
    // The dependency chain represents data flow: data flows from
    // EnumerateViewNode -> SortNode -> LimitNode, so getFirstDep()
    // on LimitNode should give us the SortNode.
    auto* dep = node->getFirstDep();
    if (dep == nullptr ||
        dep->getType() != aql::ExecutionNodeType::SORT) {
      continue;
    }

    auto* sortNode = static_cast<aql::MockSortNode*>(dep);

    // Validate the SortNode preconditions (BM25, descending).
    if (!validateSortNodeForWand(sortNode)) {
      continue;
    }

    // Step 3: Check that the SortNode's dependency is an EnumerateViewNode.
    // There must be no intervening nodes (e.g., FILTER, CALCULATION)
    // between the SortNode and the EnumerateViewNode. Any such node would
    // break the guarantee that the sort is operating directly on view output.
    auto* sortDep = dep->getFirstDep();
    if (sortDep == nullptr ||
        sortDep->getType() !=
            aql::ExecutionNodeType::ENUMERATE_IRESEARCH_VIEW) {
      continue;
    }

    auto* viewNode = static_cast<aql::MockEnumerateViewNode*>(sortDep);

    // Step 4: Annotate the EnumerateViewNode with WAND parameters.
    // This is the only mutation performed by the optimizer rule.
    // No new node types are created; we only set annotation fields
    // on the existing EnumerateViewNode.
    viewNode->setWandEnabled(true);
    viewNode->setWandHeapSize(limitNode->limit());
    ++optimized;
  }

  return optimized;
}

////////////////////////////////////////////////////////////////////////////////
/// Factory: Create a WandExecutionContext from optimizer annotations.
///
/// This is the bridge between plan-time annotations and execution-time
/// WAND behavior. Called by the execution engine when setting up an
/// EnumerateViewNode that has been annotated with WAND parameters.
///
/// If WAND is enabled, creates a ScoreThresholdManager with capacity
/// equal to heapSize. If disabled, returns a context with no manager
/// (wrapIterator will pass through).
///
/// @param enabled   Whether WAND was enabled by the optimizer rule.
/// @param heapSize  The k value from the LimitNode (top-k).
/// @return A fully initialized WandExecutionContext.
////////////////////////////////////////////////////////////////////////////////

WandExecutionContext createWandContext(bool enabled, size_t heapSize) {
  WandExecutionContext ctx;
  ctx.enabled = enabled;
  ctx.heapSize = heapSize;

  if (enabled && heapSize > 0) {
    ctx.thresholdManager = std::make_shared<ScoreThresholdManager>(heapSize);
  }

  return ctx;
}

////////////////////////////////////////////////////////////////////////////////
/// Optimizer rule: registration
///
/// In the full ArangoDB build, this would call:
///   OptimizerRulesFeature::registerRule(
///     "optimize-top-k",
///     optimizeTopKRule,
///     OptimizerRule::Flags::CanBeDisabled |
///         OptimizerRule::Flags::ClusterOnly_Never,
///     OptimizerRule::Level::Pass6);
///
/// The rule is registered at Pass6 level, which runs after basic
/// optimizations (like filter pushdown) but before execution plan
/// finalization. This ensures that the execution plan structure is
/// stable when pattern detection runs.
///
/// Stub for standalone build -- registration happens at ArangoDB
/// integration time.
////////////////////////////////////////////////////////////////////////////////

void registerOptimizeTopKRule() {
  // Placeholder: actual registration requires the full ArangoDB
  // OptimizerRulesFeature which is not available in standalone builds.
  // The rule function signature would be:
  //   void optimizeTopKRule(Optimizer* optimizer,
  //                         std::unique_ptr<ExecutionPlan> plan,
  //                         OptimizerRule const& rule);
}

}  // namespace iresearch
}  // namespace arangodb
