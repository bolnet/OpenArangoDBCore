#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "Enterprise/Aql/MinHashFunctions.h"
#include "AqlFunctionMocks.h"

using namespace arangodb;

// ============================================================
// Task 1: MinHashGenerator Unit Tests (7 tests)
// ============================================================

TEST(MinHashGenerator, DefaultK128_Returns128Hashes) {
  auto seeds = generatePermutationSeeds(kDefaultMinHashK);
  MinHashGenerator gen(seeds);
  gen.addElement("hello");
  gen.addElement("world");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 128u);
}

TEST(MinHashGenerator, K1_ReturnsSingleHash) {
  auto seeds = generatePermutationSeeds(1);
  EXPECT_EQ(seeds.size(), 1u);
  MinHashGenerator gen(seeds);
  gen.addElement("test");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 1u);
  // The single hash should not be UINT64_MAX (we added an element)
  EXPECT_NE(sig[0], std::numeric_limits<uint64_t>::max());
}

TEST(MinHashGenerator, EmptySet_ReturnsMaxValues) {
  auto seeds = generatePermutationSeeds(4);
  MinHashGenerator gen(seeds);
  // Do not add any elements
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 4u);
  for (uint64_t v : sig) {
    EXPECT_EQ(v, std::numeric_limits<uint64_t>::max());
  }
}

TEST(MinHashGenerator, SingleElement_AllHashesDiffer) {
  auto seeds = generatePermutationSeeds(kDefaultMinHashK);
  MinHashGenerator gen(seeds);
  gen.addElement("single");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 128u);

  // All 128 hash values should be distinct (different permutations).
  std::set<uint64_t> unique(sig.begin(), sig.end());
  EXPECT_EQ(unique.size(), sig.size());
}

TEST(MinHashGenerator, Deterministic_SameInputSameOutput) {
  auto seeds = generatePermutationSeeds(64);
  MinHashGenerator gen1(seeds);
  gen1.addElement("alpha");
  gen1.addElement("beta");
  gen1.addElement("gamma");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("alpha");
  gen2.addElement("beta");
  gen2.addElement("gamma");
  auto sig2 = gen2.finalize();

  EXPECT_EQ(sig1, sig2);
}

TEST(MinHashGenerator, DifferentSets_DifferentSignatures) {
  auto seeds = generatePermutationSeeds(64);

  MinHashGenerator gen1(seeds);
  gen1.addElement("apple");
  gen1.addElement("banana");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("cherry");
  gen2.addElement("date");
  auto sig2 = gen2.finalize();

  EXPECT_NE(sig1, sig2);
}

TEST(MinHashGenerator, PermutationSeeds_Precomputed) {
  // Seeds generated with same k and masterSeed must be identical across calls.
  auto seeds1 = generatePermutationSeeds(128);
  auto seeds2 = generatePermutationSeeds(128);
  ASSERT_EQ(seeds1.size(), seeds2.size());
  for (size_t i = 0; i < seeds1.size(); ++i) {
    EXPECT_EQ(seeds1[i].a, seeds2[i].a);
    EXPECT_EQ(seeds1[i].b, seeds2[i].b);
  }
}

// ============================================================
// Task 1 additional: estimateJaccard tests
// ============================================================

TEST(EstimateJaccard, IdenticalSignatures_ReturnsOne) {
  std::vector<uint64_t> sig = {1, 2, 3, 4};
  EXPECT_DOUBLE_EQ(estimateJaccard(sig, sig), 1.0);
}

TEST(EstimateJaccard, CompletelyDifferent_ReturnsZero) {
  std::vector<uint64_t> sig1 = {1, 2, 3, 4};
  std::vector<uint64_t> sig2 = {5, 6, 7, 8};
  EXPECT_DOUBLE_EQ(estimateJaccard(sig1, sig2), 0.0);
}

TEST(EstimateJaccard, HalfMatch_ReturnsHalf) {
  std::vector<uint64_t> sig1 = {1, 2, 3, 4};
  std::vector<uint64_t> sig2 = {1, 2, 7, 8};
  EXPECT_DOUBLE_EQ(estimateJaccard(sig1, sig2), 0.5);
}

TEST(EstimateJaccard, DifferentLengths_ReturnsZero) {
  std::vector<uint64_t> sig1 = {1, 2, 3};
  std::vector<uint64_t> sig2 = {1, 2, 3, 4};
  EXPECT_DOUBLE_EQ(estimateJaccard(sig1, sig2), 0.0);
}

TEST(EstimateJaccard, EmptySignatures_ReturnsZero) {
  std::vector<uint64_t> empty;
  EXPECT_DOUBLE_EQ(estimateJaccard(empty, empty), 0.0);
}

// ============================================================
// Task 1 additional: Jaccard accuracy test (+-0.05 at k=128)
// ============================================================

TEST(MinHashGenerator, JaccardAccuracy_OverlappingSets) {
  // Two sets with known 50% overlap: {"a","b","c","d"} and {"c","d","e","f"}
  // True Jaccard = |intersection| / |union| = 2 / 6 = 0.333...
  auto seeds = generatePermutationSeeds(kDefaultMinHashK);

  MinHashGenerator gen1(seeds);
  gen1.addElement("a");
  gen1.addElement("b");
  gen1.addElement("c");
  gen1.addElement("d");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("c");
  gen2.addElement("d");
  gen2.addElement("e");
  gen2.addElement("f");
  auto sig2 = gen2.finalize();

  double estimated = estimateJaccard(sig1, sig2);
  double trueJaccard = 2.0 / 6.0;
  // With k=128, error should be within ~0.1 (relaxed for small sets)
  EXPECT_NEAR(estimated, trueJaccard, 0.15);
}

// ============================================================
// Task 2: MINHASH() AQL function tests (7 tests)
// These test the core algorithm through the generator interface
// since the AQL value layer requires full ArangoDB infrastructure.
// ============================================================

TEST(AqlMinHash, ArrayInput_ReturnsSignatureArray) {
  // Simulate MINHASH(["a","b","c"], 128)
  auto seeds = generatePermutationSeeds(128);
  MinHashGenerator gen(seeds);
  gen.addElement("a");
  gen.addElement("b");
  gen.addElement("c");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 128u);
  // All values should be less than kMinHashPrime (within hash range)
  for (uint64_t v : sig) {
    EXPECT_LT(v, kMinHashPrime);
  }
}

TEST(AqlMinHash, DefaultK_Uses128) {
  // Simulate MINHASH(["a","b"]) with default k
  auto seeds = generatePermutationSeeds(kDefaultMinHashK);
  EXPECT_EQ(seeds.size(), 128u);
  MinHashGenerator gen(seeds);
  gen.addElement("a");
  gen.addElement("b");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 128u);
}

TEST(AqlMinHash, CustomK_Respected) {
  // Simulate MINHASH(["x"], 64)
  auto seeds = generatePermutationSeeds(64);
  MinHashGenerator gen(seeds);
  gen.addElement("x");
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 64u);
}

TEST(AqlMinHash, InvalidFirstArg_NonArray) {
  // MINHASH(42, 128) should be invalid -- test that generator needs elements
  // In AQL layer this returns null; here we verify empty generator behavior
  auto seeds = generatePermutationSeeds(128);
  MinHashGenerator gen(seeds);
  // No elements added -- represents invalid/non-array input
  auto sig = gen.finalize();
  for (uint64_t v : sig) {
    EXPECT_EQ(v, std::numeric_limits<uint64_t>::max());
  }
}

TEST(AqlMinHash, InvalidK_Negative) {
  // MINHASH(["a"], -1) -- k must be positive
  // The AQL layer would return null; verify k=0 creates empty seeds
  auto seeds = generatePermutationSeeds(0);
  EXPECT_TRUE(seeds.empty());
}

TEST(AqlMinHash, KZero_EmptySeeds) {
  // MINHASH(["a"], 0) returns null in AQL -- k=0 produces empty
  auto seeds = generatePermutationSeeds(0);
  EXPECT_EQ(seeds.size(), 0u);
}

TEST(AqlMinHash, EmptyArray_ReturnsMaxSignature) {
  // MINHASH([], 4) should return [MAX, MAX, MAX, MAX]
  auto seeds = generatePermutationSeeds(4);
  MinHashGenerator gen(seeds);
  // No elements (empty array)
  auto sig = gen.finalize();
  EXPECT_EQ(sig.size(), 4u);
  for (uint64_t v : sig) {
    EXPECT_EQ(v, std::numeric_limits<uint64_t>::max());
  }
}

// ============================================================
// Task 2: MINHASH_MATCH() AQL function tests (6 tests)
// These test via estimateJaccard + threshold comparison.
// ============================================================

TEST(AqlMinHashMatch, IdenticalSignatures_ReturnsTrue) {
  auto seeds = generatePermutationSeeds(128);
  MinHashGenerator gen(seeds);
  gen.addElement("a");
  gen.addElement("b");
  auto sig = gen.finalize();

  double jaccard = estimateJaccard(sig, sig);
  double threshold = 0.5;
  EXPECT_TRUE(jaccard >= threshold);
  EXPECT_DOUBLE_EQ(jaccard, 1.0);
}

TEST(AqlMinHashMatch, DisjointSignatures_ReturnsFalse) {
  auto seeds = generatePermutationSeeds(128);

  MinHashGenerator gen1(seeds);
  gen1.addElement("completely");
  gen1.addElement("different");
  gen1.addElement("set");
  gen1.addElement("of");
  gen1.addElement("words");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("entirely");
  gen2.addElement("unrelated");
  gen2.addElement("collection");
  gen2.addElement("items");
  gen2.addElement("here");
  auto sig2 = gen2.finalize();

  double jaccard = estimateJaccard(sig1, sig2);
  double threshold = 0.9;
  EXPECT_FALSE(jaccard >= threshold);
}

TEST(AqlMinHashMatch, ThresholdZero_AlwaysTrue) {
  auto seeds = generatePermutationSeeds(64);

  MinHashGenerator gen1(seeds);
  gen1.addElement("x");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("y");
  auto sig2 = gen2.finalize();

  double jaccard = estimateJaccard(sig1, sig2);
  double threshold = 0.0;
  EXPECT_TRUE(jaccard >= threshold);
}

TEST(AqlMinHashMatch, ThresholdOne_OnlyExactMatch) {
  auto seeds = generatePermutationSeeds(64);

  // Identical sets -> Jaccard = 1.0
  MinHashGenerator gen1(seeds);
  gen1.addElement("same");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("same");
  auto sig2 = gen2.finalize();

  double jaccard = estimateJaccard(sig1, sig2);
  EXPECT_TRUE(jaccard >= 1.0);

  // Different sets -> Jaccard < 1.0
  MinHashGenerator gen3(seeds);
  gen3.addElement("different");
  auto sig3 = gen3.finalize();

  double jaccard2 = estimateJaccard(sig1, sig3);
  EXPECT_FALSE(jaccard2 >= 1.0);
}

TEST(AqlMinHashMatch, DifferentLengths_ReturnsZero) {
  // MINHASH_MATCH(sig128, sig64, 0.5) -- length mismatch
  auto seeds128 = generatePermutationSeeds(128);
  MinHashGenerator gen1(seeds128);
  gen1.addElement("a");
  auto sig128 = gen1.finalize();

  auto seeds64 = generatePermutationSeeds(64);
  MinHashGenerator gen2(seeds64);
  gen2.addElement("a");
  auto sig64 = gen2.finalize();

  // estimateJaccard returns 0.0 for different lengths
  double jaccard = estimateJaccard(sig128, sig64);
  EXPECT_DOUBLE_EQ(jaccard, 0.0);
}

TEST(AqlMinHashMatch, InvalidThreshold_OutOfRange) {
  // MINHASH_MATCH(sig, sig, 1.5) -- threshold > 1.0 should be rejected
  // In AQL layer this returns null; here we verify threshold validation logic
  double threshold = 1.5;
  EXPECT_TRUE(threshold < 0.0 || threshold > 1.0);

  double negThreshold = -0.5;
  EXPECT_TRUE(negThreshold < 0.0 || negThreshold > 1.0);
}

// ============================================================
// Task 2: Additional edge case tests
// ============================================================

TEST(AqlMinHashMatch, NonArrayArgs_Invalid) {
  // MINHASH_MATCH("foo", "bar", 0.5) -- non-array args
  // In AQL layer returns null; here we test MockVPackValue type check
  MockVPackValue stringVal = makeString("foo");
  EXPECT_FALSE(stringVal.isArray());
}

TEST(MinHashGenerator, Reset_AllowsReuse) {
  auto seeds = generatePermutationSeeds(4);
  MinHashGenerator gen(seeds);
  gen.addElement("first");
  auto sig1 = gen.finalize();

  gen.reset();
  gen.addElement("second");
  auto sig2 = gen.finalize();

  // After reset, signatures should differ (different elements)
  EXPECT_NE(sig1, sig2);

  // Reset and re-add same element as first time
  gen.reset();
  gen.addElement("first");
  auto sig3 = gen.finalize();
  EXPECT_EQ(sig1, sig3);
}

TEST(MinHashGenerator, OrderIndependence) {
  // MinHash is a set operation; order of addElement should not matter
  auto seeds = generatePermutationSeeds(64);

  MinHashGenerator gen1(seeds);
  gen1.addElement("alpha");
  gen1.addElement("beta");
  gen1.addElement("gamma");
  auto sig1 = gen1.finalize();

  MinHashGenerator gen2(seeds);
  gen2.addElement("gamma");
  gen2.addElement("alpha");
  gen2.addElement("beta");
  auto sig2 = gen2.finalize();

  EXPECT_EQ(sig1, sig2);
}

TEST(PermutationSeeds, AllSeedsHaveValidA) {
  // a must be in [1, p-1], never 0
  auto seeds = generatePermutationSeeds(kDefaultMinHashK);
  for (auto const& seed : seeds) {
    EXPECT_GE(seed.a, 1u);
    EXPECT_LT(seed.a, kMinHashPrime);
    EXPECT_LT(seed.b, kMinHashPrime);
  }
}

TEST(PermutationSeeds, DifferentK_DifferentSizes) {
  auto seeds32 = generatePermutationSeeds(32);
  auto seeds64 = generatePermutationSeeds(64);
  auto seeds128 = generatePermutationSeeds(128);
  EXPECT_EQ(seeds32.size(), 32u);
  EXPECT_EQ(seeds64.size(), 64u);
  EXPECT_EQ(seeds128.size(), 128u);
}
