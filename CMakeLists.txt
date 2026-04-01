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
  Enterprise/IResearch/IResearchAnalyzerFeature.cpp

  # RocksDB
  Enterprise/RocksDBEngine/EncryptionProvider.cpp
  Enterprise/RocksDBEngine/RocksDBHotBackup.cpp
  Enterprise/RestHandler/RestHotBackupHandler.cpp
  Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.cpp
  Enterprise/RocksDBEngine/IndexBuilderThreadPool.cpp
  Enterprise/RocksDBEngine/ChangelogBuffer.cpp
  Enterprise/RocksDBEngine/KeySpacePartitioner.cpp
  Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp

  # Cluster
  Enterprise/Cluster/SatelliteDistribution.cpp
  Enterprise/Cluster/ReadFromFollower.cpp

  # Transaction
  Enterprise/Transaction/IgnoreNoAccessAqlTransaction.cpp
  Enterprise/Transaction/IgnoreNoAccessMethods.cpp

  # Replication
  Enterprise/Replication/ReplicationApplier.cpp
  Enterprise/Replication/SequenceNumberGenerator.cpp
  Enterprise/Replication/SequenceNumberTracker.cpp
  Enterprise/Replication/IdempotencyChecker.cpp
  Enterprise/Replication/ReplicationCheckpoint.cpp
  Enterprise/Replication/DC2DCReplicator.cpp
  Enterprise/Replication/ShardWALTailer.cpp
  Enterprise/Replication/MessageBatcher.cpp
  Enterprise/Replication/ReplicationLagMonitor.cpp
  Enterprise/Replication/TlsConnectionConfig.cpp
  Enterprise/Replication/ConnectionRetryPolicy.cpp
  Enterprise/Replication/DirectMQProtocol.cpp
  Enterprise/Replication/DirectMQClient.cpp
  Enterprise/Replication/ReplicationStatusBuilder.cpp
  Enterprise/Replication/ReplicationFeedStream.cpp
  Enterprise/Replication/CheckpointReceiver.cpp
  Enterprise/RestHandler/RestReplicationHandler.cpp

  # SSL
  Enterprise/Ssl/SslServerFeatureEE.cpp

  # License (always enabled — enterprise capability gate)
  Enterprise/License/LicenseFeature.cpp

  # VocBase
  Enterprise/VocBase/SmartGraphSchema.cpp
)

# Detect if we're being built as a subdirectory of ArangoDB
if(EXISTS "${CMAKE_SOURCE_DIR}/arangod" AND NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Built as enterprise/ inside ArangoDB source tree
  set(ARANGODB_INTEGRATION_BUILD ON)
  # Collect all include directories from the ArangoDB source tree.
  # Rather than hardcoding paths, we gather them from well-known locations.
  set(ARANGODB_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/arangod
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/Inspection/include
    ${CMAKE_SOURCE_DIR}/lib/Futures/include
    ${CMAKE_SOURCE_DIR}/lib/BuildId/include
    ${CMAKE_SOURCE_DIR}/lib/Actor/include
    ${CMAKE_SOURCE_DIR}/3rdParty/velocypack/include
    ${CMAKE_SOURCE_DIR}/3rdParty/immer
    ${CMAKE_SOURCE_DIR}/3rdParty/iresearch/core
    ${CMAKE_SOURCE_DIR}/3rdParty/iresearch/external
    ${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/include
    ${CMAKE_SOURCE_DIR}/3rdParty/abseil-cpp
    ${CMAKE_SOURCE_DIR}/3rdParty/date/include
    ${CMAKE_SOURCE_DIR}/3rdParty/fmt/include
    ${CMAKE_SOURCE_DIR}/3rdParty/s2geometry/src
    ${CMAKE_SOURCE_DIR}/3rdParty/boost
    ${CMAKE_SOURCE_DIR}/3rdParty/nghttp2/lib/includes
    ${CMAKE_SOURCE_DIR}/3rdParty/frozen/include
    ${CMAKE_SOURCE_DIR}/3rdParty/function2/include
    ${CMAKE_SOURCE_DIR}/3rdParty/taocpp-json/include
  )
else()
  set(ARANGODB_INTEGRATION_BUILD OFF)
  set(ARANGODB_INCLUDE_DIRS "")
endif()

target_include_directories(openarangodb_enterprise PUBLIC
  # Mock headers only for standalone builds (not when inside ArangoDB source tree)
  $<$<AND:$<BOOL:${BUILD_TESTING}>,$<NOT:$<BOOL:${ARANGODB_INTEGRATION_BUILD}>>>:${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks>
  ${CMAKE_CURRENT_SOURCE_DIR}   # so #include "Enterprise/..." resolves
  ${ARANGODB_INCLUDE_DIRS}      # ArangoDB internal headers (when integrated)
)

target_compile_definitions(openarangodb_enterprise PUBLIC USE_ENTERPRISE=1)
if(ARANGODB_INTEGRATION_BUILD)
  target_compile_definitions(openarangodb_enterprise PUBLIC ARANGODB_INTEGRATION_BUILD=1)
endif()

# When inside ArangoDB tree, wait for generated headers before compiling
if(ARANGODB_INTEGRATION_BUILD AND TARGET errorfiles)
  add_dependencies(openarangodb_enterprise errorfiles)
endif()

target_link_libraries(openarangodb_enterprise PUBLIC OpenSSL::Crypto)

if(ARANGODB_INTEGRATION_BUILD)
  target_compile_options(openarangodb_enterprise PRIVATE -Wall -Woverloaded-virtual)
else()
  target_compile_options(openarangodb_enterprise PRIVATE -Wall -Woverloaded-virtual -Werror)
endif()

# ============================================================
# arango_rclone: separate target required by
# arangodb/RocksDBEngine/CMakeLists.txt which links:
#   target_link_libraries(arango_rocksdb PRIVATE arango_rclone)
# ============================================================
add_library(arango_rclone STATIC
  Enterprise/RClone/RCloneFeature.cpp
  Enterprise/RClone/RCloneProcess.cpp
  Enterprise/RClone/RCloneConfig.cpp
)

target_include_directories(arango_rclone PUBLIC
  $<$<AND:$<BOOL:${BUILD_TESTING}>,$<NOT:$<BOOL:${ARANGODB_INTEGRATION_BUILD}>>>:${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks>
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${ARANGODB_INCLUDE_DIRS}
)

target_compile_definitions(arango_rclone PUBLIC USE_ENTERPRISE=1)
if(ARANGODB_INTEGRATION_BUILD)
  target_compile_definitions(arango_rclone PUBLIC ARANGODB_INTEGRATION_BUILD=1)
endif()

if(ARANGODB_INTEGRATION_BUILD AND TARGET errorfiles)
  add_dependencies(arango_rclone errorfiles)
endif()

if(ARANGODB_INTEGRATION_BUILD)
  target_compile_options(arango_rclone PRIVATE -Wall -Woverloaded-virtual)
else()
  target_compile_options(arango_rclone PRIVATE -Wall -Woverloaded-virtual -Werror)
endif()

# ============================================================
# Test suite (GoogleTest via FetchContent)
# ============================================================
option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# ============================================================
# Benchmark suite (Google Benchmark via FetchContent)
# ============================================================
option(BUILD_BENCHMARKS "Build the performance benchmark suite" OFF)
if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
