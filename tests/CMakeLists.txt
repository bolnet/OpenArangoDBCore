cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# --- Unit tests ---
add_executable(license_feature_test
  unit/LicenseFeatureTest.cpp
)
target_include_directories(license_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(license_feature_test
  PRIVATE openarangodb_enterprise GTest::gtest_main GTest::gmock
)
gtest_discover_tests(license_feature_test)

# --- Link symbol tests ---
add_executable(link_symbol_test
  link/test_link_symbols.cpp
)
target_include_directories(link_symbol_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(link_symbol_test
  PRIVATE openarangodb_enterprise arango_rclone GTest::gtest_main
)
gtest_discover_tests(link_symbol_test)

# --- Audit feature tests ---
add_executable(audit_feature_test
  unit/AuditFeatureTest.cpp
)
target_include_directories(audit_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(audit_feature_test
  PRIVATE openarangodb_enterprise GTest::gtest_main GTest::gmock
)
gtest_discover_tests(audit_feature_test)

# --- LDAP handler tests ---
# Note: This test uses mock libldap (no real LDAP dependency).
# The LDAPHandler sources are compiled directly with ARANGODB_LDAP_MOCK_MODE
# so that the mock function pointers are used instead of real libldap.
add_executable(ldap_handler_test
  unit/LDAPHandlerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Auth/LDAPHandler.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Auth/LDAPConfig.cpp
)
target_include_directories(ldap_handler_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_compile_definitions(ldap_handler_test PRIVATE ARANGODB_LDAP_MOCK_MODE=1)
target_link_libraries(ldap_handler_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(ldap_handler_test)

# --- Masking strategies tests ---
add_executable(masking_strategies_test
  unit/MaskingStrategiesTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Maskings/AttributeMasking.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Maskings/AttributeMaskingEE.cpp
)
target_include_directories(masking_strategies_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(masking_strategies_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(masking_strategies_test)

# --- Encryption provider tests ---
# Compile encryption sources directly to avoid pulling in the full enterprise
# library (which requires ArangoDB headers not available in standalone builds).
add_executable(encryption_provider_test
  unit/EncryptionProviderTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/EncryptionProvider.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp
)
target_include_directories(encryption_provider_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(encryption_provider_test
  PRIVATE GTest::gtest_main GTest::gmock OpenSSL::Crypto
)
gtest_discover_tests(encryption_provider_test)

# --- Encryption feature tests ---
add_executable(encryption_feature_test
  unit/EncryptionFeatureTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Encryption/EncryptionFeature.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Encryption/EncryptionProvider.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/EncryptionProvider.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp
)
target_include_directories(encryption_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(encryption_feature_test
  PRIVATE GTest::gtest_main GTest::gmock OpenSSL::Crypto
)
gtest_discover_tests(encryption_feature_test)

# --- SSL EE feature tests ---
add_executable(ssl_ee_feature_test
  unit/SslServerFeatureEETest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Ssl/SslServerFeatureEE.cpp
)
target_include_directories(ssl_ee_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ssl_ee_feature_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(ssl_ee_feature_test)

# --- Sharding strategy tests ---
add_executable(sharding_strategy_test
  unit/ShardingStrategyEETest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Sharding/ShardingStrategyEE.cpp
)
target_include_directories(sharding_strategy_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(sharding_strategy_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(sharding_strategy_test)

# --- SmartGraph schema tests ---
add_executable(smart_graph_schema_test
  unit/SmartGraphSchemaTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/VocBase/SmartGraphSchema.cpp
)
target_include_directories(smart_graph_schema_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(smart_graph_schema_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(smart_graph_schema_test)

# --- ReadFromFollower tests ---
add_executable(read_from_follower_test
  unit/ReadFromFollowerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Cluster/ReadFromFollower.cpp
)
target_include_directories(read_from_follower_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(read_from_follower_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(read_from_follower_test)

# --- Satellite distribution tests ---
add_executable(satellite_distribution_test
  unit/SatelliteDistributionTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Cluster/SatelliteDistribution.cpp
)
target_include_directories(satellite_distribution_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(satellite_distribution_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(satellite_distribution_test)

# --- Local traversal node tests ---
add_executable(local_traversal_node_test
  unit/LocalTraversalNodeTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Aql/LocalTraversalNode.cpp
)
target_include_directories(local_traversal_node_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(local_traversal_node_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(local_traversal_node_test)

# --- SmartGraph provider tests ---
add_executable(smart_graph_provider_test
  unit/SmartGraphProviderTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Graph/Providers/SmartGraphProvider.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Graph/Steps/SmartGraphStep.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Sharding/ShardingStrategyEE.cpp
)
target_include_directories(smart_graph_provider_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(smart_graph_provider_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(smart_graph_provider_test)

# --- IResearch TopK/WAND optimizer tests ---
add_executable(iresearch_topk_test
  unit/IResearchOptimizeTopKTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/IResearch/IResearchOptimizeTopK.cpp
)
target_include_directories(iresearch_topk_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(iresearch_topk_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(iresearch_topk_test)

# --- MinHash AQL function tests ---
add_executable(minhash_functions_test
  unit/MinHashFunctionsTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Aql/MinHashFunctions.cpp
)
target_include_directories(minhash_functions_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(minhash_functions_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(minhash_functions_test)

# --- IResearch MinHash analyzer tests ---
add_executable(iresearch_minhash_analyzer_test
  unit/IResearchMinHashAnalyzerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/IResearch/IResearchAnalyzerFeature.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Aql/MinHashFunctions.cpp
)
target_include_directories(iresearch_minhash_analyzer_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(iresearch_minhash_analyzer_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(iresearch_minhash_analyzer_test)

# --- KeySpacePartitioner tests ---
add_executable(key_space_partitioner_test
  unit/KeySpacePartitionerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/KeySpacePartitioner.cpp
)
target_include_directories(key_space_partitioner_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(key_space_partitioner_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(key_space_partitioner_test)

# --- ChangelogBuffer tests ---
add_executable(changelog_buffer_test
  unit/ChangelogBufferTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/ChangelogBuffer.cpp
)
target_include_directories(changelog_buffer_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(changelog_buffer_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(changelog_buffer_test)

# --- IndexBuilderThreadPool tests ---
add_executable(index_builder_thread_pool_test
  unit/IndexBuilderThreadPoolTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/IndexBuilderThreadPool.cpp
)
target_include_directories(index_builder_thread_pool_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(index_builder_thread_pool_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(index_builder_thread_pool_test)

# --- RocksDBBuilderIndexEE tests ---
add_executable(rocksdb_builder_index_ee_test
  unit/RocksDBBuilderIndexEETest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/IndexBuilderThreadPool.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/ChangelogBuffer.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/KeySpacePartitioner.cpp
)
target_include_directories(rocksdb_builder_index_ee_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(rocksdb_builder_index_ee_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(rocksdb_builder_index_ee_test)

# --- Hot backup engine tests ---
add_executable(hot_backup_test
  unit/RocksDBHotBackupTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBHotBackup.cpp
)
target_include_directories(hot_backup_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(hot_backup_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(hot_backup_test)

# --- Hot backup REST handler tests ---
add_executable(hot_backup_handler_test
  unit/RestHotBackupHandlerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RestHandler/RestHotBackupHandler.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBHotBackup.cpp
)
target_include_directories(hot_backup_handler_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(hot_backup_handler_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(hot_backup_handler_test)

# --- RClone process tests ---
add_executable(rclone_process_test
  unit/RCloneProcessTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RClone/RCloneProcess.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RClone/RCloneConfig.cpp
)
target_include_directories(rclone_process_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(rclone_process_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(rclone_process_test)

# --- RClone feature tests ---
add_executable(rclone_feature_test
  unit/RCloneFeatureTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RClone/RCloneFeature.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RClone/RCloneProcess.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RClone/RCloneConfig.cpp
)
target_include_directories(rclone_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(rclone_feature_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(rclone_feature_test)

# --- SequenceNumberGenerator tests ---
add_executable(sequence_number_generator_test
  unit/SequenceNumberGeneratorTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberGenerator.cpp
)
target_include_directories(sequence_number_generator_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(sequence_number_generator_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(sequence_number_generator_test)

# --- SequenceNumberTracker tests ---
add_executable(sequence_number_tracker_test
  unit/SequenceNumberTrackerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberTracker.cpp
)
target_include_directories(sequence_number_tracker_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(sequence_number_tracker_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(sequence_number_tracker_test)

# --- IdempotencyChecker tests ---
add_executable(idempotency_checker_test
  unit/IdempotencyCheckerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/IdempotencyChecker.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberTracker.cpp
)
target_include_directories(idempotency_checker_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(idempotency_checker_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(idempotency_checker_test)

# --- ReplicationCheckpoint tests ---
add_executable(replication_checkpoint_test
  unit/ReplicationCheckpointTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationCheckpoint.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberTracker.cpp
)
target_include_directories(replication_checkpoint_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(replication_checkpoint_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(replication_checkpoint_test)

# --- DC2DCReplicator feature tests ---
add_executable(dc2dc_replicator_feature_test
  unit/DC2DCReplicatorFeatureTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/DC2DCReplicator.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberGenerator.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberTracker.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/IdempotencyChecker.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationCheckpoint.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ShardWALTailer.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/MessageBatcher.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationLagMonitor.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Sharding/ShardingStrategyEE.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Cluster/SatelliteDistribution.cpp
)
target_include_directories(dc2dc_replicator_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(dc2dc_replicator_feature_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(dc2dc_replicator_feature_test)

# --- TlsConnectionConfig tests ---
add_executable(tls_connection_config_test
  unit/TlsConnectionConfigTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/TlsConnectionConfig.cpp
)
target_include_directories(tls_connection_config_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(tls_connection_config_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(tls_connection_config_test)

# --- DirectMQProtocol tests ---
add_executable(directmq_protocol_test
  unit/DirectMQProtocolTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/DirectMQProtocol.cpp
)
target_include_directories(directmq_protocol_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(directmq_protocol_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(directmq_protocol_test)

# --- DirectMQClient + ConnectionRetryPolicy tests ---
add_executable(directmq_client_test
  unit/DirectMQClientTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/DirectMQClient.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/TlsConnectionConfig.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ConnectionRetryPolicy.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/DirectMQProtocol.cpp
)
target_include_directories(directmq_client_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(directmq_client_test
  PRIVATE GTest::gtest_main GTest::gmock OpenSSL::SSL OpenSSL::Crypto
)
gtest_discover_tests(directmq_client_test)

# --- ShardWALTailer tests ---
add_executable(shard_wal_tailer_test
  unit/ShardWALTailerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ShardWALTailer.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Sharding/ShardingStrategyEE.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Cluster/SatelliteDistribution.cpp
)
target_include_directories(shard_wal_tailer_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(shard_wal_tailer_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(shard_wal_tailer_test)

# --- MessageBatcher tests ---
add_executable(message_batcher_test
  unit/MessageBatcherTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/MessageBatcher.cpp
)
target_include_directories(message_batcher_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(message_batcher_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(message_batcher_test)

# --- ReplicationLagMonitor tests ---
add_executable(replication_lag_monitor_test
  unit/ReplicationLagMonitorTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationLagMonitor.cpp
)
target_include_directories(replication_lag_monitor_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(replication_lag_monitor_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(replication_lag_monitor_test)

# --- RestReplicationHandler tests ---
add_executable(rest_replication_handler_test
  unit/RestReplicationHandlerTest.cpp
)
target_include_directories(rest_replication_handler_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(rest_replication_handler_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(rest_replication_handler_test)

# --- ReplicationStatusBuilder tests ---
add_executable(replication_status_builder_test
  unit/ReplicationStatusBuilderTest.cpp
)
target_include_directories(replication_status_builder_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(replication_status_builder_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(replication_status_builder_test)

# --- ReplicationFeedStream tests ---
add_executable(replication_feed_stream_test
  unit/ReplicationFeedStreamTest.cpp
)
target_include_directories(replication_feed_stream_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(replication_feed_stream_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(replication_feed_stream_test)

# --- CheckpointReceiver tests ---
add_executable(checkpoint_receiver_test
  unit/CheckpointReceiverTest.cpp
)
target_include_directories(checkpoint_receiver_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(checkpoint_receiver_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(checkpoint_receiver_test)

# --- IgnoreNoAccessTransaction tests ---
add_executable(ignore_no_access_transaction_test
  unit/IgnoreNoAccessTransactionTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Transaction/IgnoreNoAccessAqlTransaction.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Transaction/IgnoreNoAccessMethods.cpp
)
target_include_directories(ignore_no_access_transaction_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(ignore_no_access_transaction_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(ignore_no_access_transaction_test)

# --- ReplicationApplier tests ---
add_executable(replication_applier_test
  unit/ReplicationApplierTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationApplier.cpp
)
target_include_directories(replication_applier_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(replication_applier_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(replication_applier_test)

# --- DC2DC Integration tests ---
add_executable(dc2dc_integration_test
  unit/DC2DCIntegrationTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Replication/ReplicationApplier.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Transaction/IgnoreNoAccessAqlTransaction.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Transaction/IgnoreNoAccessMethods.cpp
)
target_include_directories(dc2dc_integration_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(dc2dc_integration_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(dc2dc_integration_test)
