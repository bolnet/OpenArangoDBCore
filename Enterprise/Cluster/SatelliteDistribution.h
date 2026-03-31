#pragma once
#ifndef ARANGODB_SATELLITE_DISTRIBUTION_H
#define ARANGODB_SATELLITE_DISTRIBUTION_H

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace arangodb {

/// Result of assigning shards for a satellite collection.
struct SatelliteShardAssignment {
  std::string shardId;              // The single shard ID
  std::string leader;               // First server is leader
  std::vector<std::string> followers;  // All other servers are followers
};

/// Registry tracking which collections are satellite collections.
class SatelliteCollectionRegistry {
 public:
  void registerSatellite(std::string const& collectionName);
  void unregisterSatellite(std::string const& collectionName);
  bool isSatelliteCollection(std::string const& name) const;
  std::unordered_set<std::string> const& all() const;
  void clear();

 private:
  std::unordered_set<std::string> _satellites;
};

/// SatelliteDistribution manages shard assignment for satellite collections.
///
/// Satellite collections have:
///   - replicationFactor stored as 0 internally ("satellite" in API)
///   - numberOfShards always 1
///   - writeConcern = number of DB-Servers (stored as 0, meaning "all")
///
/// Every DB-Server in the cluster holds a full copy of the satellite data.
/// This enables local joins without network hops.
class SatelliteDistribution {
 public:
  /// Sentinel value for satellite replication factor (internal representation).
  static constexpr uint64_t kSatelliteReplicationFactor = 0;

  /// Satellite collections always have exactly 1 shard.
  static constexpr uint32_t kSatelliteNumberOfShards = 1;

  /// Sentinel value for satellite write concern (means "all servers").
  static constexpr uint64_t kSatelliteWriteConcern = 0;

  /// Check if a replication factor value indicates a satellite collection.
  static bool isSatellite(uint64_t replicationFactor);

  /// Return the satellite replication factor value (0).
  static uint64_t satelliteReplicationFactor();

  /// Return the fixed number of shards for satellite collections (1).
  static uint32_t satelliteNumberOfShards();

  /// Compute effective write concern for a satellite collection.
  /// Returns the total number of servers (since all must acknowledge).
  static uint64_t effectiveWriteConcern(size_t numberOfDBServers);

  /// Assign shards for a satellite collection.
  /// Creates a single shard assigned to ALL provided DB-Servers.
  /// First server becomes leader; rest become followers.
  /// @param dbServers list of all DB-Servers in the cluster
  /// @param shardPrefix prefix for generating shard ID (e.g., "s")
  /// @param shardNumber unique number for this shard
  static SatelliteShardAssignment assignShards(
      std::vector<std::string> const& dbServers,
      std::string const& shardPrefix = "s",
      uint64_t shardNumber = 1);

  /// Check if an edge is a Smart-to-Satellite edge.
  /// An edge is SmartToSat if one endpoint references a collection in the
  /// satellite registry and the other references a smart collection.
  /// @param fromCollection collection name extracted from _from
  /// @param toCollection collection name extracted from _to
  /// @param registry the satellite collection registry
  static bool isSmartToSatEdge(
      std::string const& fromCollection,
      std::string const& toCollection,
      SatelliteCollectionRegistry const& registry);

  /// Extract the collection name from a document ID (format: "collection/key").
  static std::string_view extractCollectionName(std::string_view documentId);
};

}  // namespace arangodb

#endif  // ARANGODB_SATELLITE_DISTRIBUTION_H
