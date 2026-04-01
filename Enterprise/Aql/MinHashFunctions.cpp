#include "MinHashFunctions.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace arangodb {

namespace {

/// FNV-1a 64-bit hash for converting string elements to uint64.
uint64_t baseHash(std::string_view element, uint64_t seed) {
  uint64_t hash = 14695981039346656037ULL ^ seed;
  for (char c : element) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= 1099511628211ULL;
  }
  return hash;
}

/// Modular multiplication: (a * b) mod p where p = 2^61 - 1.
/// Uses __uint128_t to avoid overflow.
uint64_t mulmod(uint64_t a, uint64_t b, uint64_t p) {
  __uint128_t result = static_cast<__uint128_t>(a) * b;
  // Fast reduction for Mersenne prime 2^61 - 1
  uint64_t lo = static_cast<uint64_t>(result) & p;
  uint64_t hi = static_cast<uint64_t>(result >> 61);
  uint64_t val = lo + hi;
  return val >= p ? val - p : val;
}

/// Reduce a full 64-bit value modulo the Mersenne prime.
uint64_t reduceMod(uint64_t x, uint64_t p) {
  // For Mersenne prime p = 2^61 - 1:
  // x mod p = (x & p) + (x >> 61), then subtract p if needed.
  uint64_t lo = x & p;
  uint64_t hi = x >> 61;
  uint64_t val = lo + hi;
  return val >= p ? val - p : val;
}

/// Apply permutation hash: h(x) = (a*x + b) mod p.
uint64_t permutationHash(uint64_t x, PermutationSeed const& seed) {
  uint64_t p = kMinHashPrime;
  // Reduce x into [0, p-1] before multiplication to keep values in range.
  uint64_t xr = reduceMod(x, p);
  uint64_t ax = mulmod(seed.a, xr, p);
  uint64_t result = ax + seed.b;
  return result >= p ? result - p : result;
}

}  // namespace

// --- generatePermutationSeeds ---

std::vector<PermutationSeed> generatePermutationSeeds(
    uint32_t k, uint64_t masterSeed) {
  std::vector<PermutationSeed> seeds;
  seeds.reserve(k);
  for (uint32_t i = 0; i < k; ++i) {
    // Derive deterministic a, b from masterSeed and index.
    uint64_t a = baseHash(std::string_view(
        reinterpret_cast<char const*>(&i), sizeof(i)), masterSeed);
    uint64_t b = baseHash(std::string_view(
        reinterpret_cast<char const*>(&i), sizeof(i)), masterSeed ^ 0xDEADBEEFULL);
    // Ensure a is in [1, p-1] and b is in [0, p-1]
    a = (a % (kMinHashPrime - 1)) + 1;
    b = b % kMinHashPrime;
    seeds.push_back({a, b});
  }
  return seeds;
}

// --- MinHashGenerator ---

MinHashGenerator::MinHashGenerator(
    std::vector<PermutationSeed> const& seeds)
    : _seeds(seeds),
      _minimums(seeds.size(), std::numeric_limits<uint64_t>::max()) {}

void MinHashGenerator::addElement(std::string_view element) {
  // Base-hash the element once.
  uint64_t h = baseHash(element, 0);
  // Apply each permutation and track minimums.
  for (size_t i = 0; i < _seeds.size(); ++i) {
    uint64_t ph = permutationHash(h, _seeds[i]);
    if (ph < _minimums[i]) {
      _minimums[i] = ph;
    }
  }
}

std::vector<uint64_t> MinHashGenerator::finalize() const {
  return _minimums;
}

void MinHashGenerator::reset() {
  std::fill(_minimums.begin(), _minimums.end(),
            std::numeric_limits<uint64_t>::max());
}

// --- estimateJaccard ---

double estimateJaccard(std::vector<uint64_t> const& sig1,
                       std::vector<uint64_t> const& sig2) {
  if (sig1.size() != sig2.size() || sig1.empty()) {
    return 0.0;
  }
  uint32_t matches = 0;
  for (size_t i = 0; i < sig1.size(); ++i) {
    if (sig1[i] == sig2[i]) {
      ++matches;
    }
  }
  return static_cast<double>(matches) / static_cast<double>(sig1.size());
}

// --- AQL Function Registration ---

void registerMinHashAqlFunctions() {
  // Registration of MINHASH and MINHASH_MATCH with AqlFunctionFeature.
  // In production ArangoDB this calls:
  //   AqlFunctionFeature::addEnterpriseFunction("MINHASH", ".,.",
  //       Deterministic | Cacheable | CanRunOnDBServerCluster, &aqlMinHash);
  //   AqlFunctionFeature::addEnterpriseFunction("MINHASH_MATCH", ".,.,.",
  //       Deterministic | Cacheable | CanRunOnDBServerCluster, &aqlMinHashMatch);
  // Flags: Deterministic=0x01, Cacheable=0x02, CanRunOnDBServerCluster=0x04
}

}  // namespace arangodb
