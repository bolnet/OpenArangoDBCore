#include "Enterprise/Sharding/ShardingStrategyEE.h"
#include <string>
#include <string_view>

namespace arangodb::graph {

/// Enterprise path validation for disjoint SmartGraph traversals.
/// Validates that each step in a traversal path stays within the same
/// smart graph partition (same smartGraphAttribute value).

struct PathValidationResult {
  bool valid;
  std::string errorMessage;

  static PathValidationResult ok() { return {true, ""}; }
  static PathValidationResult invalid(std::string msg) {
    return {false, std::move(msg)};
  }
};

struct PathValidatorOptions {
  bool isDisjoint = false;
  bool isSatelliteLeader = false;
  bool clusterOneShardRuleEnabled = false;
};

/// Check if a traversal step is valid for a disjoint SmartGraph.
/// In disjoint mode, all vertices in a traversal must share the same
/// smart prefix (smartGraphAttribute value).
PathValidationResult checkValidDisjointPath(
    std::string_view startVertexKey, std::string_view currentVertexKey,
    PathValidatorOptions const& options) {
  if (!options.isDisjoint) {
    return PathValidationResult::ok();
  }

  auto startPrefix = arangodb::extractSmartPrefix(startVertexKey);
  auto currentPrefix = arangodb::extractSmartPrefix(currentVertexKey);

  if (startPrefix != currentPrefix) {
    return PathValidationResult::invalid(
        "Disjoint SmartGraph violation: vertex '" +
        std::string(currentVertexKey) + "' has smart prefix '" +
        std::string(currentPrefix) + "' but expected '" +
        std::string(startPrefix) + "'");
  }

  return PathValidationResult::ok();
}

}  // namespace arangodb::graph
