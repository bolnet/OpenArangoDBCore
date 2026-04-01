#include <benchmark/benchmark.h>

#include "Enterprise/RocksDBEngine/EncryptionProvider.h"

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Key and IV helpers
// ---------------------------------------------------------------------------

static std::string make32ByteKey() {
  // Deterministic 32-byte key for benchmarking (NOT for production)
  return std::string(32, '\xAB');
}

static std::string make16ByteIV() {
  // Deterministic 16-byte IV for benchmarking
  return std::string(16, '\xCD');
}

static std::vector<char> generateRandomData(size_t bytes, uint64_t seed = 42) {
  std::mt19937_64 rng(seed);
  std::vector<char> data(bytes);
  auto* ptr = reinterpret_cast<uint64_t*>(data.data());
  size_t count = bytes / sizeof(uint64_t);
  for (size_t i = 0; i < count; ++i) {
    ptr[i] = rng();
  }
  // Fill remaining bytes
  for (size_t i = count * sizeof(uint64_t); i < bytes; ++i) {
    data[i] = static_cast<char>(rng() & 0xFF);
  }
  return data;
}

// ---------------------------------------------------------------------------
// BM_AES256CTREncrypt: encryption throughput at various block sizes
// ---------------------------------------------------------------------------

static void BM_AES256CTREncrypt(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto const key = make32ByteKey();
  auto const iv = make16ByteIV();
  arangodb::enterprise::AESCTRCipherStream stream(key, iv);

  auto data = generateRandomData(blockSize);

  for (auto _ : state) {
    // Encrypt in-place; reset data each iteration by working on a copy
    auto buf = data;
    auto status = stream.Encrypt(0, buf.data(), buf.size());
    benchmark::DoNotOptimize(buf.data());
    benchmark::DoNotOptimize(status.ok());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize));
}

BENCHMARK(BM_AES256CTREncrypt)
    ->Arg(4096)          // 4 KB
    ->Arg(16384)         // 16 KB
    ->Arg(65536)         // 64 KB
    ->Arg(262144)        // 256 KB
    ->Arg(1048576)       // 1 MB
    ->Arg(4194304)       // 4 MB
    ->Arg(16777216)      // 16 MB
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_AES256CTRDecrypt: decryption throughput at various block sizes
// ---------------------------------------------------------------------------

static void BM_AES256CTRDecrypt(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto const key = make32ByteKey();
  auto const iv = make16ByteIV();
  arangodb::enterprise::AESCTRCipherStream stream(key, iv);

  // Pre-encrypt the data
  auto data = generateRandomData(blockSize);
  stream.Encrypt(0, data.data(), data.size());

  for (auto _ : state) {
    auto buf = data;
    auto status = stream.Decrypt(0, buf.data(), buf.size());
    benchmark::DoNotOptimize(buf.data());
    benchmark::DoNotOptimize(status.ok());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize));
}

BENCHMARK(BM_AES256CTRDecrypt)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(1048576)
    ->Arg(16777216)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_AES256CTRRandomOffset: encrypt at random file offsets (CTR mode)
// ---------------------------------------------------------------------------

static void BM_AES256CTRRandomOffset(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto const key = make32ByteKey();
  auto const iv = make16ByteIV();
  arangodb::enterprise::AESCTRCipherStream stream(key, iv);

  auto data = generateRandomData(blockSize);
  std::mt19937_64 rng(99);
  // Random offsets within a hypothetical 1 GB file
  std::uniform_int_distribution<uint64_t> offsetDist(0, 1ULL << 30);

  for (auto _ : state) {
    auto buf = data;
    uint64_t offset = offsetDist(rng);
    auto status = stream.Encrypt(offset, buf.data(), buf.size());
    benchmark::DoNotOptimize(buf.data());
    benchmark::DoNotOptimize(status.ok());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize));
}

BENCHMARK(BM_AES256CTRRandomOffset)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(1048576)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_EncryptionProviderCreatePrefix: IV generation overhead
// ---------------------------------------------------------------------------

static void BM_EncryptionProviderCreatePrefix(benchmark::State& state) {
  auto const key = make32ByteKey();
  arangodb::enterprise::EncryptionProvider provider(key);

  auto const prefixLen = provider.GetPrefixLength();
  std::vector<char> prefix(prefixLen, 0);

  for (auto _ : state) {
    auto status =
        provider.CreateNewPrefix("test.sst", prefix.data(), prefixLen);
    benchmark::DoNotOptimize(prefix.data());
    benchmark::DoNotOptimize(status.ok());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_EncryptionProviderCreatePrefix);

// ---------------------------------------------------------------------------
// BM_EncryptedVsUnencryptedWrite: compare encrypted vs memcpy (baseline)
// ---------------------------------------------------------------------------

static void BM_UnencryptedWriteBaseline(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto source = generateRandomData(blockSize);
  std::vector<char> dest(blockSize);

  for (auto _ : state) {
    std::memcpy(dest.data(), source.data(), blockSize);
    benchmark::DoNotOptimize(dest.data());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize));
}

BENCHMARK(BM_UnencryptedWriteBaseline)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(1048576)
    ->Arg(16777216)
    ->Unit(benchmark::kMicrosecond);

static void BM_EncryptedWrite(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto const key = make32ByteKey();
  auto const iv = make16ByteIV();
  arangodb::enterprise::AESCTRCipherStream stream(key, iv);

  auto source = generateRandomData(blockSize);

  for (auto _ : state) {
    // Copy + encrypt (simulates an encrypted write path)
    auto buf = source;
    auto status = stream.Encrypt(0, buf.data(), buf.size());
    benchmark::DoNotOptimize(buf.data());
    benchmark::DoNotOptimize(status.ok());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize));
}

BENCHMARK(BM_EncryptedWrite)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(1048576)
    ->Arg(16777216)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_EncryptDecryptRoundtrip: full encrypt then decrypt cycle
// ---------------------------------------------------------------------------

static void BM_EncryptDecryptRoundtrip(benchmark::State& state) {
  auto const blockSize = static_cast<size_t>(state.range(0));
  auto const key = make32ByteKey();
  auto const iv = make16ByteIV();
  arangodb::enterprise::AESCTRCipherStream stream(key, iv);

  auto data = generateRandomData(blockSize);

  for (auto _ : state) {
    auto buf = data;
    stream.Encrypt(0, buf.data(), buf.size());
    stream.Decrypt(0, buf.data(), buf.size());
    benchmark::DoNotOptimize(buf.data());
  }
  state.SetBytesProcessed(
      static_cast<int64_t>(state.iterations()) *
      static_cast<int64_t>(blockSize) * 2);  // read + write
}

BENCHMARK(BM_EncryptDecryptRoundtrip)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(1048576)
    ->Unit(benchmark::kMicrosecond);
