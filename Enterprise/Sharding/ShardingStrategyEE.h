#pragma once
#ifndef ARANGODB_SHARDING_STRATEGY_EE_H
#define ARANGODB_SHARDING_STRATEGY_EE_H

#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb {

// --- Helper functions ---

/// Extract the substring before the first ':' in a key.
/// If no ':' is found, returns the entire key.
std::string_view extractSmartPrefix(std::string_view key);

/// Compute a shard index using FNV-1a 32-bit hash mod numberOfShards.
uint32_t computeShardIndex(std::string_view value, uint32_t numberOfShards);

/// Registration hook for enterprise sharding strategies.
void registerEnterpriseShardingStrategies();

// --- Strategy classes ---

/// Sharding strategy for SmartGraph edge collections.
/// Extracts the first prefix from edge keys of the form "fromSmart:toSmart:edgeKey"
/// and hashes it to determine the responsible shard.
class EnterpriseHashSmartEdgeShardingStrategy {
 public:
  std::string const& name() const;
  bool usesDefaultShardKeys() const;
  uint32_t getResponsibleShard(std::string_view key,
                               uint32_t numberOfShards) const;
};

/// Sharding strategy for EnterpriseGraph vertex collections.
/// Extracts the prefix before ':' and hashes it. If no ':', hashes the full key.
class EnterpriseHexSmartVertexShardingStrategy {
 public:
  std::string const& name() const;
  bool usesDefaultShardKeys() const;
  uint32_t getResponsibleShard(std::string_view key,
                               uint32_t numberOfShards) const;
};

/// Legacy (pre-3.4) smart edge sharding strategy.
/// Behaves identically to EnterpriseHashSmartEdgeShardingStrategy.
class EnterpriseSmartEdgeCompatShardingStrategy {
 public:
  std::string const& name() const;
  bool usesDefaultShardKeys() const;
  uint32_t getResponsibleShard(std::string_view key,
                               uint32_t numberOfShards) const;
};

}  // namespace arangodb

#endif  // ARANGODB_SHARDING_STRATEGY_EE_H
