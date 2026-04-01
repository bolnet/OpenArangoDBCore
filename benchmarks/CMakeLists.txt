cmake_minimum_required(VERSION 3.20)

# ============================================================
# Google Benchmark via FetchContent (same pattern as GoogleTest)
# ============================================================
include(FetchContent)

FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# ============================================================
# Helper function: reduces boilerplate for each benchmark target
# ============================================================
function(add_bench TARGET_NAME)
  cmake_parse_arguments(BENCH "" "" "SOURCES;LIBS" ${ARGN})

  add_executable(${TARGET_NAME} ${BENCH_SOURCES})

  target_include_directories(${TARGET_NAME}
    BEFORE PRIVATE
      ${CMAKE_SOURCE_DIR}/tests/mocks
    PRIVATE
      ${CMAKE_SOURCE_DIR}
  )

  target_compile_definitions(${TARGET_NAME} PRIVATE USE_ENTERPRISE=1)

  target_link_libraries(${TARGET_NAME}
    PRIVATE benchmark::benchmark benchmark::benchmark_main ${BENCH_LIBS}
  )
endfunction()

# ============================================================
# SmartGraph benchmarks
# ============================================================
add_bench(smart_graph_benchmark
  SOURCES
    SmartGraphBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Sharding/ShardingStrategyEE.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/VocBase/SmartGraphSchema.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Cluster/SatelliteDistribution.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Graph/Providers/SmartGraphProvider.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Graph/Steps/SmartGraphStep.cpp
  LIBS
)

# ============================================================
# MinHash benchmarks
# ============================================================
add_bench(minhash_benchmark
  SOURCES
    MinHashBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Aql/MinHashFunctions.cpp
  LIBS
)

# ============================================================
# TopK / WAND benchmarks
# ============================================================
add_bench(topk_benchmark
  SOURCES
    TopKBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/IResearch/IResearchOptimizeTopK.cpp
  LIBS
)

# ============================================================
# Encryption benchmarks
# ============================================================
add_bench(encryption_benchmark
  SOURCES
    EncryptionBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/EncryptionProvider.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp
  LIBS
    OpenSSL::Crypto
)

# ============================================================
# Parallel index building benchmarks
# ============================================================
add_bench(parallel_index_benchmark
  SOURCES
    ParallelIndexBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/KeySpacePartitioner.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/ChangelogBuffer.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/IndexBuilderThreadPool.cpp
  LIBS
)

# ============================================================
# Replication benchmarks
# ============================================================
add_bench(replication_benchmark
  SOURCES
    ReplicationBenchmark.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberGenerator.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Replication/SequenceNumberTracker.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Replication/IdempotencyChecker.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Replication/MessageBatcher.cpp
    ${CMAKE_SOURCE_DIR}/Enterprise/Replication/DirectMQProtocol.cpp
  LIBS
)

# ============================================================
# Convenience target: build all benchmarks at once
# ============================================================
add_custom_target(all_benchmarks DEPENDS
  smart_graph_benchmark
  minhash_benchmark
  topk_benchmark
  encryption_benchmark
  parallel_index_benchmark
  replication_benchmark
)
