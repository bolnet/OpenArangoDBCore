#include "AttributeMasking.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

// Minimal JSON parsing for masking config.
// We use a simple approach: parse the config JSON manually since
// we cannot depend on VelocyPack in this standalone module.
// For production ArangoDB integration, the base Maskings framework
// handles VelocyPack-based config; this code handles the JSON file
// format used by arangodump's --maskings option.

namespace arangodb::maskings {

// ============================================================
// AttributeMasking registry (static)
// ============================================================

std::unordered_map<std::string, MaskingFactory>& AttributeMasking::registry() {
  static std::unordered_map<std::string, MaskingFactory> instance;
  return instance;
}

void AttributeMasking::installMasking(std::string const& name,
                                      MaskingFactory factory) {
  registry()[name] = std::move(factory);
}

MaskingFactory const* AttributeMasking::findMasking(
    std::string const& name) {
  auto& reg = registry();
  auto it = reg.find(name);
  if (it == reg.end()) return nullptr;
  return &it->second;
}

void AttributeMasking::clearMaskings() {
  registry().clear();
}

std::vector<std::string> AttributeMasking::registeredNames() {
  std::vector<std::string> names;
  for (auto const& [name, _] : registry()) {
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

// ============================================================
// Simple JSON config parser
// ============================================================

namespace {

// Trim whitespace from both ends.
[[maybe_unused]] std::string_view trim(std::string_view sv) {
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
    sv.remove_suffix(1);
  }
  return sv;
}

// Extract a quoted string value starting at pos (after the opening quote).
// Returns the string content and advances pos past the closing quote.
std::string extractString(std::string const& json, size_t& pos) {
  std::string result;
  while (pos < json.size() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < json.size()) {
      ++pos;
    }
    result += json[pos];
    ++pos;
  }
  if (pos < json.size()) ++pos;  // skip closing quote
  return result;
}

// Skip whitespace.
void skipWs(std::string const& json, size_t& pos) {
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
}

// Find the next occurrence of a character, skipping nested structures.
size_t findMatchingBrace(std::string const& json, size_t start, char open,
                         char close) {
  int depth = 1;
  size_t pos = start;
  while (pos < json.size() && depth > 0) {
    if (json[pos] == '"') {
      ++pos;
      while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\') ++pos;
        ++pos;
      }
      if (pos < json.size()) ++pos;
      continue;
    }
    if (json[pos] == open) ++depth;
    if (json[pos] == close) --depth;
    if (depth > 0) ++pos;
  }
  return pos;
}

}  // namespace

std::vector<CollectionMaskingConfig> AttributeMasking::loadConfigFromJson(
    std::string const& json) {
  std::vector<CollectionMaskingConfig> result;

  // Find "collections" key
  auto collectionsPos = json.find("\"collections\"");
  if (collectionsPos == std::string::npos) {
    return result;
  }

  // Find the opening brace of the collections object
  auto bracePos = json.find('{', collectionsPos + 14);
  if (bracePos == std::string::npos) return result;

  size_t collectionsEnd = findMatchingBrace(json, bracePos + 1, '{', '}');

  // Parse each collection
  size_t pos = bracePos + 1;
  while (pos < collectionsEnd) {
    skipWs(json, pos);
    if (pos >= collectionsEnd || json[pos] != '"') break;

    // Collection name
    ++pos;
    std::string collName = extractString(json, pos);
    skipWs(json, pos);
    if (pos < json.size() && json[pos] == ':') ++pos;
    skipWs(json, pos);

    CollectionMaskingConfig config;
    config.collectionName = collName;

    // Find "rules" array
    auto rulesKeyPos = json.find("\"rules\"", pos);
    if (rulesKeyPos != std::string::npos &&
        rulesKeyPos < collectionsEnd) {
      auto arrayStart = json.find('[', rulesKeyPos);
      if (arrayStart != std::string::npos) {
        auto arrayEnd = findMatchingBrace(json, arrayStart + 1, '[', ']');

        // Parse each rule object
        size_t rulePos = arrayStart + 1;
        while (rulePos < arrayEnd) {
          skipWs(json, rulePos);
          if (rulePos >= arrayEnd || json[rulePos] != '{') {
            ++rulePos;
            continue;
          }

          auto ruleEnd = findMatchingBrace(json, rulePos + 1, '{', '}');
          std::string ruleStr = json.substr(rulePos, ruleEnd - rulePos + 1);

          MaskingRule rule;

          // Extract "path"
          auto pathPos = ruleStr.find("\"path\"");
          if (pathPos != std::string::npos) {
            auto qStart = ruleStr.find('"', pathPos + 6);
            if (qStart != std::string::npos) {
              size_t qPos = qStart + 1;
              rule.path = extractString(ruleStr, qPos);
            }
          }

          // Extract "type"
          auto typePos = ruleStr.find("\"type\"");
          if (typePos != std::string::npos) {
            auto qStart = ruleStr.find('"', typePos + 6);
            if (qStart != std::string::npos) {
              size_t qPos = qStart + 1;
              rule.type = extractString(ruleStr, qPos);
            }
          }

          // Extract "roles" array (optional)
          auto rolesPos = ruleStr.find("\"roles\"");
          if (rolesPos != std::string::npos) {
            auto rolesArrayStart = ruleStr.find('[', rolesPos);
            if (rolesArrayStart != std::string::npos) {
              auto rolesArrayEnd =
                  findMatchingBrace(ruleStr, rolesArrayStart + 1, '[', ']');
              size_t rp = rolesArrayStart + 1;
              while (rp < rolesArrayEnd) {
                skipWs(ruleStr, rp);
                if (rp < rolesArrayEnd && ruleStr[rp] == '"') {
                  ++rp;
                  rule.roles.push_back(extractString(ruleStr, rp));
                } else {
                  ++rp;
                }
              }
            }
          }

          if (!rule.path.empty() && !rule.type.empty()) {
            config.rules.push_back(std::move(rule));
          }

          rulePos = ruleEnd + 1;
          skipWs(json, rulePos);
          if (rulePos < arrayEnd && json[rulePos] == ',') ++rulePos;
        }

        pos = arrayEnd + 1;
      }
    }

    // Skip to after this collection's closing brace
    auto collObjStart = json.find('{', json.find('"' + collName + '"'));
    if (collObjStart != std::string::npos) {
      pos = findMatchingBrace(json, collObjStart + 1, '{', '}') + 1;
    }

    result.push_back(std::move(config));

    skipWs(json, pos);
    if (pos < collectionsEnd && json[pos] == ',') ++pos;
  }

  return result;
}

std::unordered_map<std::string, std::string> AttributeMasking::applyMasking(
    std::string const& collectionName,
    std::string const& userRole,
    std::unordered_map<std::string, std::string> const& fields,
    std::vector<CollectionMaskingConfig> const& configs) {

  auto result = fields;  // copy -- immutable pattern

  for (auto const& config : configs) {
    if (config.collectionName != collectionName) continue;

    for (auto const& rule : config.rules) {
      if (!rule.appliesToRole(userRole)) continue;

      auto it = result.find(rule.path);
      if (it == result.end()) continue;

      auto const* factory = findMasking(rule.type);
      if (factory == nullptr) continue;

      auto maskFn = (*factory)();
      it->second = maskFn->mask(it->second);
    }
  }

  return result;
}

// ============================================================
// Enterprise Masking Strategy Implementations
// ============================================================

std::string XifyFrontMask::mask(std::string_view input) const {
  std::string result;
  result.reserve(input.size());
  for (char ch : input) {
    if (ch == ' ') {
      result += ' ';
    } else {
      result += 'x';
    }
  }
  return result;
}

std::string EmailMask::mask(std::string_view input) const {
  // Produce a deterministic hashed email in "AAAA.BBBB@CCCC.invalid" format.
  // Use a simple hash (FNV-1a) for reproducibility.
  auto fnv1a = [](std::string_view data) -> uint32_t {
    uint32_t hash = 2166136261u;
    for (char c : data) {
      hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
      hash *= 16777619u;
    }
    return hash;
  };

  uint32_t h = fnv1a(input);
  // Generate 3 segments from the hash
  auto toAlpha = [](uint32_t val, int len) -> std::string {
    std::string s;
    for (int i = 0; i < len; ++i) {
      s += static_cast<char>('A' + (val % 26));
      val /= 26;
    }
    return s;
  };

  std::string part1 = toAlpha(h, 4);
  std::string part2 = toAlpha(h >> 8, 4);
  std::string part3 = toAlpha(h >> 16, 4);

  return part1 + "." + part2 + "@" + part3 + ".invalid";
}

std::string CreditCardMask::mask(std::string_view input) const {
  // Mask all but last 4 digits with 'x'.
  // Non-digit characters are preserved.
  std::string result(input);

  // Count digits
  int digitCount = 0;
  for (char ch : input) {
    if (std::isdigit(static_cast<unsigned char>(ch))) ++digitCount;
  }

  // Mask all digits except last 4
  int digitsToMask = digitCount > 4 ? digitCount - 4 : 0;
  int masked = 0;
  for (auto& ch : result) {
    if (std::isdigit(static_cast<unsigned char>(ch)) &&
        masked < digitsToMask) {
      ch = 'x';
      ++masked;
    }
  }
  return result;
}

std::string PhoneMask::mask(std::string_view input) const {
  // Mask all but last 4 digits, preserving separators.
  // Example: "+1-555-123-4567" -> "xxx-xxx-xxx-4567"
  std::string result(input);

  // Count digits
  int digitCount = 0;
  for (char ch : input) {
    if (std::isdigit(static_cast<unsigned char>(ch))) ++digitCount;
  }

  // Mask all digits except last 4; replace non-digit non-separator chars too
  int digitsToMask = digitCount > 4 ? digitCount - 4 : 0;
  int masked = 0;
  for (auto& ch : result) {
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      if (masked < digitsToMask) {
        ch = 'x';
        ++masked;
      }
    } else if (ch != '-' && ch != '(' && ch != ')' && ch != ' ' &&
               ch != '.') {
      // Non-separator, non-digit character (like '+') -- mask it
      ch = 'x';
    }
  }
  return result;
}

}  // namespace arangodb::maskings
