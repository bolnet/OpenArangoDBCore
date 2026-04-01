#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Enterprise/IResearch/IResearchAnalyzerFeature.h"
#include "Enterprise/Aql/MinHashFunctions.h"

using namespace arangodb;

// ============================================================
// Task 3: MinHash Analyzer Tests (8+ tests)
// ============================================================

TEST(MinHashAnalyzer, Construct_WithDefaults) {
  MinHashAnalyzerConfig config;
  MinHashAnalyzer analyzer(config);
  EXPECT_EQ(analyzer.config().numHashes, 128u);
  EXPECT_EQ(analyzer.config().analyzerType, "delimiter");
}

TEST(MinHashAnalyzer, Construct_WithCustomConfig) {
  MinHashAnalyzerConfig config;
  config.numHashes = 64;
  config.analyzerType = "delimiter";
  config.analyzerProperties = R"({"delimiter": ","})";
  MinHashAnalyzer analyzer(config);
  EXPECT_EQ(analyzer.config().numHashes, 64u);
  EXPECT_EQ(analyzer.config().analyzerType, "delimiter");
}

TEST(MinHashAnalyzer, Reset_TokenizesInput) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  MinHashAnalyzer analyzer(config);

  EXPECT_TRUE(analyzer.reset("hello world foo bar"));

  // Should be able to get tokens via next()
  std::vector<std::string_view> tokens;
  while (analyzer.next()) {
    tokens.push_back(analyzer.value());
  }
  EXPECT_FALSE(tokens.empty());
}

TEST(MinHashAnalyzer, SignatureTokenCount_EqualsK) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  MinHashAnalyzer analyzer(config);

  analyzer.reset("hello world");

  size_t count = 0;
  while (analyzer.next()) {
    ++count;
  }
  EXPECT_EQ(count, 4u);
}

TEST(MinHashAnalyzer, Deterministic_SameInput) {
  MinHashAnalyzerConfig config;
  config.numHashes = 8;
  MinHashAnalyzer analyzer(config);

  // First pass
  analyzer.reset("the quick brown fox");
  std::vector<std::string> tokens1;
  while (analyzer.next()) {
    tokens1.emplace_back(analyzer.value());
  }

  // Second pass with same input
  analyzer.reset("the quick brown fox");
  std::vector<std::string> tokens2;
  while (analyzer.next()) {
    tokens2.emplace_back(analyzer.value());
  }

  ASSERT_EQ(tokens1.size(), tokens2.size());
  for (size_t i = 0; i < tokens1.size(); ++i) {
    EXPECT_EQ(tokens1[i], tokens2[i]);
  }
}

TEST(MinHashAnalyzer, Normalize_ValidConfig) {
  std::string out;
  bool ok = MinHashAnalyzer::normalize("", out);
  EXPECT_TRUE(ok);
  // Should produce canonical default config
  EXPECT_EQ(out, R"({"numHashes":128,"analyzer":{"type":"delimiter","properties":{"delimiter":" "}}})");
}

TEST(MinHashAnalyzer, Normalize_EmptyBraces) {
  std::string out;
  bool ok = MinHashAnalyzer::normalize("{}", out);
  EXPECT_TRUE(ok);
  EXPECT_EQ(out, R"({"numHashes":128,"analyzer":{"type":"delimiter","properties":{"delimiter":" "}}})");
}

TEST(MinHashAnalyzer, Normalize_InvalidConfig) {
  std::string out;
  // numHashes = -1 is invalid
  bool ok = MinHashAnalyzer::normalize(R"({"numHashes": -1})", out);
  EXPECT_FALSE(ok);
}

TEST(MinHashAnalyzer, Normalize_NumHashesTooLarge) {
  std::string out;
  bool ok = MinHashAnalyzer::normalize(R"({"numHashes": 99999})", out);
  EXPECT_FALSE(ok);
}

TEST(MinHashAnalyzer, Make_ReturnsInstance) {
  auto analyzer = MinHashAnalyzer::make("");
  ASSERT_NE(analyzer, nullptr);
  EXPECT_EQ(analyzer->config().numHashes, 128u);
}

TEST(MinHashAnalyzer, Make_WithCustomNumHashes) {
  auto analyzer = MinHashAnalyzer::make(R"({"numHashes": 64})");
  ASSERT_NE(analyzer, nullptr);
  EXPECT_EQ(analyzer->config().numHashes, 64u);
}

TEST(MinHashAnalyzer, Make_DefaultBraces) {
  auto analyzer = MinHashAnalyzer::make("{}");
  ASSERT_NE(analyzer, nullptr);
  EXPECT_EQ(analyzer->config().numHashes, 128u);
}

TEST(MinHashAnalyzer, TypeName) {
  EXPECT_EQ(MinHashAnalyzer::type_name(), "minhash");
}

TEST(MinHashAnalyzer, TokensHaveSlotPrefix) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  MinHashAnalyzer analyzer(config);

  analyzer.reset("hello world");

  std::vector<std::string> tokens;
  while (analyzer.next()) {
    tokens.emplace_back(analyzer.value());
  }

  ASSERT_EQ(tokens.size(), 4u);
  // Each token should start with "slot:" prefix
  for (size_t i = 0; i < tokens.size(); ++i) {
    std::string prefix = std::to_string(i) + ":";
    EXPECT_EQ(tokens[i].substr(0, prefix.size()), prefix)
        << "Token " << i << " should start with '" << prefix << "' but is: " << tokens[i];
  }
}

TEST(MinHashAnalyzer, EmptyInput_ReturnsTrue) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  MinHashAnalyzer analyzer(config);

  // Empty input -- no tokens to hash, but reset should still succeed
  bool ok = analyzer.reset("");
  EXPECT_TRUE(ok);

  // No tokens should be emitted (empty input -> no tokenizer output -> empty sig tokens)
  // Wait -- empty tokens but reset returns true because tokens.empty() is true
  size_t count = 0;
  while (analyzer.next()) {
    ++count;
  }
  // With the current implementation, empty input produces no tokens from tokenizer,
  // so MinHashGenerator gets no elements, signature is all UINT64_MAX,
  // and we still emit k signature tokens
  // Actually, let's check: if tokens is empty, we still call gen.finalize()
  // which produces k MAX values, then we convert to strings. So count should be k.
  EXPECT_EQ(count, 4u);
}

TEST(MinHashAnalyzer, ValueBeforeNext_ReturnsEmpty) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  MinHashAnalyzer analyzer(config);

  analyzer.reset("hello world");

  // Before calling next(), value() should return empty
  EXPECT_TRUE(analyzer.value().empty());
}

TEST(MinHashAnalyzer, CustomDelimiter) {
  MinHashAnalyzerConfig config;
  config.numHashes = 4;
  config.analyzerType = "delimiter";
  config.analyzerProperties = R"({"delimiter": ","})";
  MinHashAnalyzer analyzer(config);

  analyzer.reset("hello,world,foo,bar");

  size_t count = 0;
  while (analyzer.next()) {
    ++count;
  }
  EXPECT_EQ(count, 4u);
}
