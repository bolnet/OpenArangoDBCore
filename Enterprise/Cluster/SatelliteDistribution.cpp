#include "SatelliteDistribution.h"

namespace arangodb {

// --- SatelliteCollectionRegistry ---

void SatelliteCollectionRegistry::registerSatellite(std::string const& collectionName) {
  _satellites.insert(collectionName);
}

void SatelliteCollectionRegistry::unregisterSatellite(std::string const& collectionName) {
  _satellites.erase(collectionName);
}

bool SatelliteCollectionRegistry::isSatelliteCollection(std::string const& name) const {
  return _satellites.count(name) > 0;
}

std::unordered_set<std::string> const& SatelliteCollectionRegistry::all() const {
  return _satellites;
}

void SatelliteCollectionRegistry::clear() {
  _satellites.clear();
}

// --- SatelliteDistribution ---

bool SatelliteDistribution::isSatellite(uint64_t replicationFactor) {
  return replicationFactor == kSatelliteReplicationFactor;
}

uint64_t SatelliteDistribution::satelliteReplicationFactor() {
  return kSatelliteReplicationFactor;
}

uint32_t SatelliteDistribution::satelliteNumberOfShards() {
  return kSatelliteNumberOfShards;
}

uint64_t SatelliteDistribution::effectiveWriteConcern(size_t numberOfDBServers) {
  return static_cast<uint64_t>(numberOfDBServers);
}

SatelliteShardAssignment SatelliteDistribution::assignShards(
    std::vector<std::string> const& dbServers,
    std::string const& shardPrefix,
    uint64_t shardNumber) {
  SatelliteShardAssignment result;
  result.shardId = shardPrefix + std::to_string(shardNumber);

  if (!dbServers.empty()) {
    result.leader = dbServers[0];
    for (size_t i = 1; i < dbServers.size(); ++i) {
      result.followers.push_back(dbServers[i]);
    }
  }

  return result;
}

bool SatelliteDistribution::isSmartToSatEdge(
    std::string const& fromCollection,
    std::string const& toCollection,
    SatelliteCollectionRegistry const& registry) {
  bool fromIsSatellite = registry.isSatelliteCollection(fromCollection);
  bool toIsSatellite = registry.isSatelliteCollection(toCollection);

  // SmartToSat: exactly one side is satellite, the other is not
  return fromIsSatellite != toIsSatellite;
}

std::string_view SatelliteDistribution::extractCollectionName(std::string_view documentId) {
  auto pos = documentId.find('/');
  if (pos == std::string_view::npos) {
    return documentId;
  }
  return documentId.substr(0, pos);
}

}  // namespace arangodb
