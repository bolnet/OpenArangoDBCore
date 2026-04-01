#include "IResearchAnalyzerFeature.h"
#include "Enterprise/Aql/MinHashFunctions.h"
#include <sstream>
#include <stdexcept>

namespace arangodb {

MinHashAnalyzer::MinHashAnalyzer(MinHashAnalyzerConfig config)
    : _config(std::move(config)) {}

std::unique_ptr<MinHashAnalyzer> MinHashAnalyzer::make(
    std::string_view args) {
  MinHashAnalyzerConfig config;

  if (args.empty() || args == "{}") {
    return std::make_unique<MinHashAnalyzer>(config);
  }

  // Minimal JSON parsing for numHashes field.
  // Production code would use VelocyPack or a full JSON parser.
  std::string input(args);
  auto pos = input.find("\"numHashes\"");
  if (pos != std::string::npos) {
    auto colonPos = input.find(':', pos);
    if (colonPos != std::string::npos) {
      auto valueStart = input.find_first_of("0123456789", colonPos);
      if (valueStart != std::string::npos) {
        auto valueEnd = input.find_first_not_of("0123456789", valueStart);
        std::string numStr = input.substr(valueStart, valueEnd - valueStart);
        int64_t val = std::stoll(numStr);
        if (val <= 0 || val > static_cast<int64_t>(kMaxMinHashK)) {
          return nullptr;  // invalid numHashes
        }
        config.numHashes = static_cast<uint32_t>(val);
      }
    }
  }

  // Parse analyzer type if present
  auto typePos = input.find("\"type\"");
  if (typePos != std::string::npos) {
    auto typeColonPos = input.find(':', typePos);
    if (typeColonPos != std::string::npos) {
      auto quoteStart = input.find('"', typeColonPos + 1);
      if (quoteStart != std::string::npos) {
        auto quoteEnd = input.find('"', quoteStart + 1);
        if (quoteEnd != std::string::npos) {
          config.analyzerType = input.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
      }
    }
  }

  // Parse delimiter if present
  auto delimPos = input.find("\"delimiter\"");
  if (delimPos != std::string::npos) {
    auto delimColonPos = input.find(':', delimPos);
    if (delimColonPos != std::string::npos) {
      auto quoteStart = input.find('"', delimColonPos + 1);
      if (quoteStart != std::string::npos) {
        auto quoteEnd = input.find('"', quoteStart + 1);
        if (quoteEnd != std::string::npos) {
          std::string delim = input.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
          config.analyzerProperties = R"({"delimiter": ")" + delim + R"("})";
        }
      }
    }
  }

  return std::make_unique<MinHashAnalyzer>(config);
}

bool MinHashAnalyzer::normalize(std::string_view args, std::string& out) {
  // Empty or default config -> produce canonical form
  if (args.empty() || args == "{}") {
    out = R"({"numHashes":128,"analyzer":{"type":"delimiter","properties":{"delimiter":" "}}})";
    return true;
  }

  // Validate that the config contains numHashes if provided
  std::string input(args);
  auto pos = input.find("\"numHashes\"");
  if (pos != std::string::npos) {
    auto colonPos = input.find(':', pos);
    if (colonPos != std::string::npos) {
      auto valueStart = input.find_first_of("-0123456789", colonPos);
      if (valueStart != std::string::npos) {
        try {
          int64_t val = std::stoll(input.substr(valueStart));
          if (val <= 0 || val > static_cast<int64_t>(kMaxMinHashK)) {
            return false;  // invalid numHashes value
          }
        } catch (...) {
          return false;
        }
      }
    }
  }

  out = std::string(args);
  return true;
}

bool MinHashAnalyzer::reset(std::string_view data) {
  _signatureTokens.clear();
  _currentToken = 0;

  // Step 1: Tokenize input using inner analyzer (space-delimited by default)
  std::vector<std::string> tokens;

  if (_config.analyzerType == "delimiter") {
    // Extract delimiter from config
    std::string delimiter = " ";
    auto delimPos = _config.analyzerProperties.find("\"delimiter\"");
    if (delimPos != std::string::npos) {
      auto colonPos = _config.analyzerProperties.find(':', delimPos);
      if (colonPos != std::string::npos) {
        auto quoteStart = _config.analyzerProperties.find('"', colonPos + 1);
        if (quoteStart != std::string::npos) {
          auto quoteEnd = _config.analyzerProperties.find('"', quoteStart + 1);
          if (quoteEnd != std::string::npos) {
            delimiter = _config.analyzerProperties.substr(
                quoteStart + 1, quoteEnd - quoteStart - 1);
          }
        }
      }
    }

    if (delimiter == " ") {
      // Space tokenizer: split on whitespace
      std::string dataStr{data};
      std::istringstream stream{dataStr};
      std::string token;
      while (stream >> token) {
        tokens.push_back(std::move(token));
      }
    } else {
      // Custom delimiter tokenizer
      std::string input{data};
      size_t start = 0;
      size_t end = input.find(delimiter);
      while (end != std::string::npos) {
        std::string tok = input.substr(start, end - start);
        if (!tok.empty()) {
          tokens.push_back(std::move(tok));
        }
        start = end + delimiter.size();
        end = input.find(delimiter, start);
      }
      std::string last = input.substr(start);
      if (!last.empty()) {
        tokens.push_back(std::move(last));
      }
    }
  }

  // Step 2: Compute MinHash signature
  auto seeds = generatePermutationSeeds(_config.numHashes);
  MinHashGenerator gen(seeds);
  for (auto const& t : tokens) {
    gen.addElement(t);
  }
  auto signature = gen.finalize();

  // Step 3: Convert signature values to string tokens for indexing.
  // Prefix with slot index for positional matching:
  // "slot:value" ensures slot 0 of sig1 is compared with slot 0 of sig2.
  _signatureTokens.reserve(signature.size());
  for (size_t i = 0; i < signature.size(); ++i) {
    _signatureTokens.push_back(
        std::to_string(i) + ":" + std::to_string(signature[i]));
  }

  return !_signatureTokens.empty() || tokens.empty();
}

bool MinHashAnalyzer::next() {
  if (_currentToken >= _signatureTokens.size()) {
    return false;
  }
  ++_currentToken;
  return true;
}

std::string_view MinHashAnalyzer::value() const {
  if (_currentToken == 0 || _currentToken > _signatureTokens.size()) {
    return {};
  }
  return _signatureTokens[_currentToken - 1];
}

void registerMinHashAnalyzer() {
  // Register "minhash" analyzer type with IResearch registry.
  // In production this calls:
  //   irs::analysis::analyzers::reg<MinHashAnalyzer>("minhash");
  // Actual registration depends on IResearch API availability.
}

}  // namespace arangodb
