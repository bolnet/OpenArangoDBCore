#include "SmartGraphStep.h"
#include "Enterprise/Sharding/ShardingStrategyEE.h"

namespace arangodb::graph {

std::string_view SmartGraphStep::getSmartPrefix() const {
  // Vertex ID format: "collection/smartPrefix:localKey"
  auto pos = _vertexId.find('/');
  if (pos == std::string_view::npos) {
    return arangodb::extractSmartPrefix(_vertexId);
  }
  return arangodb::extractSmartPrefix(
      std::string_view(_vertexId).substr(pos + 1));
}

}  // namespace arangodb::graph
