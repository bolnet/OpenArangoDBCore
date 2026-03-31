cmake_minimum_required(VERSION 3.20)
project(OpenArangoDBCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL is required for AES-256-CTR encryption at rest
find_package(OpenSSL REQUIRED)

# This project is compiled as part of ArangoDB with -DUSE_ENTERPRISE=1
# It provides the Enterprise/ directory symbols.
# All sources must be under Enterprise/ (not the old arangod path).

# ============================================================
# openarangodb_enterprise: all enterprise feature objects
# ============================================================
add_library(openarangodb_enterprise STATIC
  # Audit
  Enterprise/Audit/AuditFeature.cpp
  Enterprise/Audit/AuditLogger.cpp

  # Encryption
  Enterprise/Encryption/EncryptionFeature.cpp
  Enterprise/Encryption/EncryptionProvider.cpp

  # Sharding
  Enterprise/Sharding/ShardingStrategyEE.cpp

  # Graph
  Enterprise/Graph/PathValidatorEE.cpp
  Enterprise/Graph/Providers/SmartGraphProvider.cpp
  Enterprise/Graph/Steps/SmartGraphStep.cpp

  # AQL extensions
  Enterprise/Aql/LocalTraversalNode.cpp
  Enterprise/Aql/MinHashFunctions.cpp

  # Auth
  Enterprise/Auth/LDAPConfig.cpp
  Enterprise/Auth/LDAPHandler.cpp

  # Maskings
  Enterprise/Maskings/AttributeMasking.cpp
  Enterprise/Maskings/AttributeMaskingEE.cpp

  # IResearch
  Enterprise/IResearch/IResearchOptimizeTopK.cpp

  # RocksDB
  Enterprise/RocksDBEngine/EncryptionProvider.cpp
  Enterprise/RocksDBEngine/RocksDBHotBackup.cpp
  Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.cpp
  Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp

  # Cluster
  Enterprise/Cluster/SatelliteDistribution.cpp
  Enterprise/Cluster/ReadFromFollower.cpp

  # Replication
  Enterprise/Replication/DC2DCReplicator.cpp

  # SSL
  Enterprise/Ssl/SslServerFeatureEE.cpp

  # License (always enabled — enterprise capability gate)
  Enterprise/License/LicenseFeature.cpp

  # VocBase
  Enterprise/VocBase/SmartGraphSchema.cpp
)

target_include_directories(openarangodb_enterprise PUBLIC
  $<$<BOOL:${BUILD_TESTING}>:${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks>
  ${CMAKE_CURRENT_SOURCE_DIR}   # so #include "Enterprise/..." resolves
  ${CMAKE_SOURCE_DIR}           # so ArangoDB internal headers resolve
)

target_compile_definitions(openarangodb_enterprise PUBLIC USE_ENTERPRISE=1)

target_link_libraries(openarangodb_enterprise PUBLIC OpenSSL::Crypto)

target_compile_options(openarangodb_enterprise PRIVATE
  -Wall
  -Woverloaded-virtual
  -Werror
)

# ============================================================
# arango_rclone: separate target required by
# arangodb/RocksDBEngine/CMakeLists.txt which links:
#   target_link_libraries(arango_rocksdb PRIVATE arango_rclone)
# ============================================================
add_library(arango_rclone STATIC
  Enterprise/RClone/RCloneFeature.cpp
)

target_include_directories(arango_rclone PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(arango_rclone PUBLIC USE_ENTERPRISE=1)

target_compile_options(arango_rclone PRIVATE
  -Wall
  -Woverloaded-virtual
  -Werror
)

# ============================================================
# Test suite (GoogleTest via FetchContent)
# ============================================================
option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
