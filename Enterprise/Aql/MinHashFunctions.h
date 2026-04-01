#pragma once
#ifndef ARANGODB_MIN_HASH_FUNCTIONS_H
#define ARANGODB_MIN_HASH_FUNCTIONS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {

/// Default number of hash functions for MinHash signatures.
static constexpr uint32_t kDefaultMinHashK = 128;

/// Large Mersenne prime for modular arithmetic in hash family.
/// p = 2^61 - 1 (largest Mersenne prime fitting in uint64).
static constexpr uint64_t kMinHashPrime = (1ULL << 61) - 1;

/// Maximum allowed value of k to prevent memory abuse.
static constexpr uint32_t kMaxMinHashK = 4096;

/// A single permutation in the universal hash family: h(x) = (a*x + b) mod p.
struct PermutationSeed {
  uint64_t a;  // multiplier (must be in [1, p-1])
  uint64_t b;  // offset (must be in [0, p-1])
};

/// Precomputed set of k permutation seeds for MinHash computation.
/// Seeds are deterministically generated from a master seed so that
/// all nodes in a cluster produce identical signatures for the same input.
std::vector<PermutationSeed> generatePermutationSeeds(
    uint32_t k, uint64_t masterSeed = 0x9E3779B97F4A7C15ULL);

/// MinHashGenerator computes MinHash signatures over arbitrary string sets.
///
/// Usage:
///   auto seeds = generatePermutationSeeds(128);
///   MinHashGenerator gen(seeds);
///   gen.addElement("foo");
///   gen.addElement("bar");
///   auto sig = gen.finalize();  // vector of 128 uint64 values
class MinHashGenerator {
 public:
  /// Construct with precomputed permutation seeds.
  /// Does NOT take ownership -- caller must keep seeds alive.
  explicit MinHashGenerator(std::vector<PermutationSeed> const& seeds);

  /// Hash a single set element and update running minimums.
  void addElement(std::string_view element);

  /// Return the final signature (vector of k minimum hash values).
  /// After calling finalize(), the generator is consumed; do not reuse.
  std::vector<uint64_t> finalize() const;

  /// Reset the generator to compute a new signature with the same seeds.
  void reset();

  /// Number of hash functions (k).
  uint32_t k() const { return static_cast<uint32_t>(_minimums.size()); }

 private:
  std::vector<PermutationSeed> const& _seeds;
  std::vector<uint64_t> _minimums;  // running min for each hash function
};

/// Compute estimated Jaccard similarity from two MinHash signatures.
/// Returns value in [0.0, 1.0]. Signatures must have equal length.
double estimateJaccard(std::vector<uint64_t> const& sig1,
                       std::vector<uint64_t> const& sig2);

// ---- AQL Function Registration ----

/// Register MINHASH and MINHASH_MATCH with the AQL function registry.
/// Called from AqlFunctionFeature::addEnterpriseFunctions().
void registerMinHashAqlFunctions();

}  // namespace arangodb

#endif  // ARANGODB_MIN_HASH_FUNCTIONS_H
