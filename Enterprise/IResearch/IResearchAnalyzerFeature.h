#pragma once
#ifndef ARANGODB_IRESEARCH_ANALYZER_FEATURE_H
#define ARANGODB_IRESEARCH_ANALYZER_FEATURE_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {

/// Configuration for the MinHash IResearch analyzer.
struct MinHashAnalyzerConfig {
  uint32_t numHashes = 128;
  std::string analyzerType = "delimiter";  // inner tokenizer type
  std::string analyzerProperties = R"({"delimiter": " "})";  // inner tokenizer config
};

/// MinHash analyzer for IResearch integration.
///
/// Pipeline:
///   1. Inner tokenizer splits the field value into a set of tokens
///   2. MinHashGenerator computes k signatures over the token set
///   3. Each signature value is emitted as a synthetic "token" for indexing
///
/// This allows MINHASH_MATCH queries to be evaluated against indexed
/// signatures rather than computing MinHash at query time.
class MinHashAnalyzer {
 public:
  static constexpr std::string_view type_name() { return "minhash"; }

  explicit MinHashAnalyzer(MinHashAnalyzerConfig config);

  /// IResearch analyzer factory: create instance from JSON config string.
  static std::unique_ptr<MinHashAnalyzer> make(std::string_view args);

  /// Normalize configuration to canonical form.
  /// Returns true on success, false if config is invalid.
  static bool normalize(std::string_view args, std::string& out);

  /// Reset the analyzer with new input data.
  /// Tokenizes input with inner analyzer, computes MinHash signature.
  bool reset(std::string_view data);

  /// Advance to the next signature token.
  /// Returns false when all k tokens have been emitted.
  bool next();

  /// Current token value (string representation of uint64 signature component).
  std::string_view value() const;

  /// Access the full config.
  MinHashAnalyzerConfig const& config() const { return _config; }

 private:
  MinHashAnalyzerConfig _config;
  std::vector<std::string> _signatureTokens;  // k stringified hash values
  size_t _currentToken = 0;
};

/// Register the MinHash analyzer with the IResearch analyzer registry.
/// Called from IResearchAnalyzerFeature::prepare() or equivalent startup.
void registerMinHashAnalyzer();

}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH_ANALYZER_FEATURE_H
