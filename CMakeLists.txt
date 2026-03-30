cmake_minimum_required(VERSION 3.20)
project(OpenArangoDBCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This project is compiled as part of ArangoDB with -DUSE_ENTERPRISE=1
# It provides the Enterprise/ directory symbols

add_library(openarangodb_enterprise STATIC
  # Audit
  arangod/Audit/AuditFeature.cpp

  # Encryption
  arangod/Encryption/EncryptionFeature.cpp
  arangod/Encryption/EncryptionProvider.cpp

  # Sharding
  arangod/Sharding/ShardingStrategyEE.cpp

  # Graph
  arangod/Graph/Providers/SmartGraphProvider.cpp
  arangod/Graph/Steps/SmartGraphStep.cpp

  # AQL extensions
  arangod/Aql/LocalTraversalNode.cpp
  arangod/Aql/MinHashFunctions.cpp

  # Auth
  arangod/Auth/LDAPHandler.cpp

  # Maskings
  arangod/Maskings/AttributeMasking.cpp

  # IResearch
  arangod/IResearch/IResearchOptimizeTopK.cpp

  # RocksDB
  arangod/RocksDBEngine/RocksDBHotBackup.cpp
  arangod/RocksDBEngine/RocksDBBuilderIndexEE.cpp
  arangod/RocksDBEngine/RocksDBEncryptionUtils.cpp

  # Cluster
  arangod/Cluster/SatelliteDistribution.cpp
  arangod/Cluster/ReadFromFollower.cpp

  # Replication
  arangod/Replication/DC2DCReplicator.cpp

  # RClone
  arangod/RClone/RCloneFeature.cpp

  # SSL
  arangod/Ssl/SslServerFeatureEE.cpp

  # License (always enabled)
  arangod/License/LicenseFeature.cpp

  # VocBase
  arangod/VocBase/SmartGraphSchema.cpp
)

target_include_directories(openarangodb_enterprise PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}  # ArangoDB root for shared headers
)

target_compile_definitions(openarangodb_enterprise PUBLIC USE_ENTERPRISE=1)
