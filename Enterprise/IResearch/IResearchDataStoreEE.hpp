#pragma once
#ifndef ARANGODB_IRESEARCH_DATA_STORE_EE_HPP
#define ARANGODB_IRESEARCH_DATA_STORE_EE_HPP

#include <cstddef>
#include <memory>

namespace arangodb {
namespace iresearch {

// Forward declarations
class ScoredDocIterator;
class ScoreThresholdManager;

////////////////////////////////////////////////////////////////////////////////
/// @struct WandExecutionContext
/// @brief Context passed to IResearch data store when WAND is enabled on a
/// view node.
///
/// Bridges the optimizer rule annotation to the execution-time iterator
/// wrapping. Created by the execution engine when an EnumerateViewNode has
/// _enableWand=true, and passed to the IResearch data store to wrap the
/// posting list iterator with a WandIterator.
////////////////////////////////////////////////////////////////////////////////
struct WandExecutionContext {
  /// Whether WAND early termination is enabled for this execution.
  bool enabled = false;

  /// The k value for top-k tracking (from LimitNode::limit()).
  size_t heapSize = 0;

  /// Shared threshold manager that tracks the running k-th best score.
  /// Shared between the WandIterator and the execution engine so that
  /// threshold updates from scored documents propagate immediately.
  std::shared_ptr<ScoreThresholdManager> thresholdManager;

  /// Create a WandIterator wrapping the given base iterator.
  /// Returns the base iterator unchanged if WAND is not enabled.
  std::unique_ptr<ScoredDocIterator> wrapIterator(
      std::unique_ptr<ScoredDocIterator> base) const;
};

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH_DATA_STORE_EE_HPP
