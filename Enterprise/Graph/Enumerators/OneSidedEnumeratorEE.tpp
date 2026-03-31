#pragma once

// Enterprise extension for OneSidedEnumerator template.
// In ArangoDB, this provides smartSearch() for SmartGraph traversals
// via conditional compilation with SmartGraphStep.
//
// Template usage pattern (from ArangoDB):
//   using StepType = std::conditional_t<is_smart, SmartGraphStep,
//                                       SingleServerStep>;
//   OneSidedEnumerator<Config> enumerator;
//   if constexpr (is_smart) { enumerator.smartSearch(...); }

namespace arangodb::graph::enterprise {

/// SmartSearchHelper dispatches traversal to SmartGraphProvider for
/// smart-aware neighbor enumeration. Called from OneSidedEnumerator when
/// the graph is a SmartGraph.
///
/// @tparam Provider the graph provider type (SmartGraphProvider for smart
/// graphs)
/// @tparam Step the step type (SmartGraphStep)
template <typename Provider, typename Step>
struct SmartSearchHelper {
  /// Execute a smart search from the given vertex at the specified depth.
  /// The provider determines which shard owns the vertex and routes
  /// the expansion accordingly.
  static bool smartSearch(Provider& provider, Step const& currentStep,
                          uint64_t depth) {
    // In a full implementation, this would:
    // 1. Check if currentStep is local via provider.isResponsible()
    // 2. If local: expand directly from local RocksDB
    // 3. If remote: queue RPC request to remote DB-Server
    // For standalone compilation, we provide the interface.
    return currentStep.isLocal();
  }
};

}  // namespace arangodb::graph::enterprise
