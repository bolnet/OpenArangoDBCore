#include "Enterprise/Sharding/ShardingStrategyEE.h"

namespace arangodb {

// --- Strategy name constants ---

static std::string const kEnterpriseHashSmartEdge =
    "enterprise-hash-smart-edge";
static std::string const kEnterpriseHexSmartVertex =
    "enterprise-hex-smart-vertex";
static std::string const kEnterpriseSmartEdgeCompat =
    "enterprise-smart-edge-compat";

// --- Helper functions ---

std::string_view extractSmartPrefix(std::string_view key) {
  auto pos = key.find(':');
  if (pos == std::string_view::npos) {
    return key;
  }
  return key.substr(0, pos);
}

uint32_t computeShardIndex(std::string_view value, uint32_t numberOfShards) {
  // FNV-1a 32-bit hash
  constexpr uint32_t kFnvOffsetBasis = 2166136261u;
  constexpr uint32_t kFnvPrime = 16777619u;

  uint32_t hash = kFnvOffsetBasis;
  for (auto c : value) {
    hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
    hash *= kFnvPrime;
  }
  return hash % numberOfShards;
}

void registerEnterpriseShardingStrategies() {
  // Registration hook — in a full ArangoDB build this would register
  // the strategies with the ShardingStrategyFactory. In the standalone
  // enterprise library it is a no-op placeholder.
}

// --- EnterpriseHashSmartEdgeShardingStrategy ---

std::string const& EnterpriseHashSmartEdgeShardingStrategy::name() const {
  return kEnterpriseHashSmartEdge;
}

bool EnterpriseHashSmartEdgeShardingStrategy::usesDefaultShardKeys() const {
  return false;
}

uint32_t EnterpriseHashSmartEdgeShardingStrategy::getResponsibleShard(
    std::string_view key, uint32_t numberOfShards) const {
  // Edge key format: "fromSmart:toSmart:edgeKey"
  // We extract the first prefix (fromSmart) and hash it.
  auto prefix = extractSmartPrefix(key);
  return computeShardIndex(prefix, numberOfShards);
}

// --- EnterpriseHexSmartVertexShardingStrategy ---

std::string const& EnterpriseHexSmartVertexShardingStrategy::name() const {
  return kEnterpriseHexSmartVertex;
}

bool EnterpriseHexSmartVertexShardingStrategy::usesDefaultShardKeys() const {
  return false;
}

uint32_t EnterpriseHexSmartVertexShardingStrategy::getResponsibleShard(
    std::string_view key, uint32_t numberOfShards) const {
  // Vertex key format: "smartPrefix:localKey"
  // Extract prefix before ':', hash it. If no ':', hash full key.
  auto prefix = extractSmartPrefix(key);
  return computeShardIndex(prefix, numberOfShards);
}

// --- EnterpriseSmartEdgeCompatShardingStrategy ---

std::string const& EnterpriseSmartEdgeCompatShardingStrategy::name() const {
  return kEnterpriseSmartEdgeCompat;
}

bool EnterpriseSmartEdgeCompatShardingStrategy::usesDefaultShardKeys() const {
  return false;
}

uint32_t EnterpriseSmartEdgeCompatShardingStrategy::getResponsibleShard(
    std::string_view key, uint32_t numberOfShards) const {
  // Legacy compat: same behaviour as EnterpriseHashSmartEdge
  auto prefix = extractSmartPrefix(key);
  return computeShardIndex(prefix, numberOfShards);
}

}  // namespace arangodb
