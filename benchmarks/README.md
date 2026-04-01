# OpenArangoDBCore Performance Benchmarks

Microbenchmark suite for validating performance of key enterprise modules.
Built with [Google Benchmark](https://github.com/google/benchmark) (fetched
automatically via CMake FetchContent).

## Modules Covered

| Benchmark File | Module | What It Measures |
|---|---|---|
| SmartGraphBenchmark.cpp | Sharding / SmartGraph | FNV-1a hash throughput, smart key validation, disjoint edge checks, satellite replication factor |
| MinHashBenchmark.cpp | AQL MinHash | Signature generation at various set sizes, MINHASH_MATCH comparison, Jaccard estimation accuracy |
| TopKBenchmark.cpp | IResearch WAND/TopK | ScoreThresholdManager insert/check throughput, WandIterator skip rates, optimizer pattern detection |
| EncryptionBenchmark.cpp | Encryption at Rest | AES-256-CTR throughput at various block sizes, key derivation overhead, encrypted vs unencrypted write path |
| ParallelIndexBenchmark.cpp | RocksDB Parallel Index | KeySpacePartitioner speed, ChangelogBuffer write throughput, IndexBuilderThreadPool task latency, thread scaling |
| ReplicationBenchmark.cpp | DC2DC Replication | SequenceNumberGenerator throughput, IdempotencyChecker accept/reject, MessageBatcher batching, DirectMQProtocol framing/parsing |

## Building

From the project root:

```bash
cmake -B build -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON
cmake --build build --target all_benchmarks
```

Or build individual benchmarks:

```bash
cmake --build build --target smart_graph_benchmark
cmake --build build --target minhash_benchmark
cmake --build build --target topk_benchmark
cmake --build build --target encryption_benchmark
cmake --build build --target parallel_index_benchmark
cmake --build build --target replication_benchmark
```

## Running

Run all benchmarks:

```bash
./build/benchmarks/smart_graph_benchmark
./build/benchmarks/minhash_benchmark
./build/benchmarks/topk_benchmark
./build/benchmarks/encryption_benchmark
./build/benchmarks/parallel_index_benchmark
./build/benchmarks/replication_benchmark
```

### Common Options

```bash
# JSON output for CI integration
./build/benchmarks/smart_graph_benchmark --benchmark_format=json

# Filter specific benchmarks
./build/benchmarks/minhash_benchmark --benchmark_filter="BM_MinHashSignature"

# Control iteration count
./build/benchmarks/encryption_benchmark --benchmark_min_time=5s

# Save results to file
./build/benchmarks/topk_benchmark --benchmark_out=results.json --benchmark_out_format=json
```

### Comparing Results

Use Google Benchmark's `compare.py` tool to compare before/after:

```bash
./build/benchmarks/smart_graph_benchmark --benchmark_out=before.json --benchmark_out_format=json
# ... make changes ...
./build/benchmarks/smart_graph_benchmark --benchmark_out=after.json --benchmark_out_format=json
# Compare (requires google-benchmark Python tools):
python3 -m google_benchmark.compare before.json after.json
```

## Interpreting Results

- **Time** columns show wall-clock and CPU time per iteration.
- **items_per_second** (where set) shows throughput in domain-meaningful units.
- **bytes_per_second** (where set) shows data throughput (e.g., encryption MB/s).
- Benchmark names encode parameters: e.g., `BM_MinHashSignature/100` means set size 100.

## Adding New Benchmarks

1. Create `benchmarks/NewModuleBenchmark.cpp`.
2. Include `<benchmark/benchmark.h>` and relevant Enterprise headers.
3. Add a new target in `benchmarks/CMakeLists.txt` following the existing pattern.
4. Add to `all_benchmarks` custom target dependencies.
