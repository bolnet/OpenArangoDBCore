#pragma once
#ifndef ARANGODB_ATTRIBUTE_MASKING_H
#define ARANGODB_ATTRIBUTE_MASKING_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::maskings {

// Forward declarations
class MaskingFunction;

/// A single masking rule: field path + strategy name + optional role filter.
struct MaskingRule {
  std::string path;       // e.g. "email", "ssn", "address.zip"
  std::string type;       // strategy name: "xifyFront", "email", etc.
  std::vector<std::string> roles;  // empty = applies to all roles

  bool appliesToRole(std::string const& role) const {
    if (roles.empty()) return true;
    for (auto const& r : roles) {
      if (r == role) return true;
    }
    return false;
  }
};

/// Per-collection masking configuration.
struct CollectionMaskingConfig {
  std::string collectionName;
  std::vector<MaskingRule> rules;
};

/// Factory function type for creating masking strategy instances.
using MaskingFactory = std::function<std::unique_ptr<MaskingFunction>()>;

/// Base class for masking strategy functions.
/// Each strategy transforms a string input into a masked output.
class MaskingFunction {
 public:
  virtual ~MaskingFunction() = default;

  /// Apply the masking strategy to input, returning masked string.
  virtual std::string mask(std::string_view input) const = 0;
};

/// Field-level masking with per-collection, per-role configuration.
/// This is the enterprise masking framework that extends ArangoDB's
/// base masking with configurable strategies.
class AttributeMasking {
 public:
  /// Register a named masking strategy factory.
  /// Called by InstallMaskingsEE() to add enterprise strategies.
  static void installMasking(std::string const& name, MaskingFactory factory);

  /// Look up a registered masking factory by name.
  /// Returns nullptr if not found.
  static MaskingFactory const* findMasking(std::string const& name);

  /// Clear all registered masking strategies (for testing).
  static void clearMaskings();

  /// Get a list of all registered strategy names.
  static std::vector<std::string> registeredNames();

  /// Load masking configuration from a JSON string.
  /// JSON format:
  /// {
  ///   "collections": {
  ///     "users": {
  ///       "rules": [
  ///         { "path": "email", "type": "email" },
  ///         { "path": "ssn", "type": "xifyFront", "roles": ["viewer"] }
  ///       ]
  ///     }
  ///   }
  /// }
  static std::vector<CollectionMaskingConfig> loadConfigFromJson(
      std::string const& json);

  /// Apply masking rules to a set of field key-value pairs.
  /// Returns a new map with masked values for matched fields.
  /// @param collectionName  the collection being queried
  /// @param userRole        the current user's role
  /// @param fields          field name -> field value pairs
  /// @param configs         the loaded masking configurations
  static std::unordered_map<std::string, std::string> applyMasking(
      std::string const& collectionName,
      std::string const& userRole,
      std::unordered_map<std::string, std::string> const& fields,
      std::vector<CollectionMaskingConfig> const& configs);

 private:
  static std::unordered_map<std::string, MaskingFactory>& registry();
};

// ============================================================
// Enterprise Masking Strategy Classes
// ============================================================

/// XifyFront: replaces each character with 'x', preserving spaces and length.
/// Example: "John Doe" -> "xxxx xxx"
class XifyFrontMask final : public MaskingFunction {
 public:
  std::string mask(std::string_view input) const override;
};

/// EmailMask: produces a deterministic hashed email in
/// "AAAA.BBBB@CCCC.invalid" format.
/// The hash is reproducible for the same input.
class EmailMask final : public MaskingFunction {
 public:
  std::string mask(std::string_view input) const override;
};

/// CreditCardMask: masks all but last 4 digits.
/// Example: "4111111111111111" -> "xxxxxxxxxxxx1111"
class CreditCardMask final : public MaskingFunction {
 public:
  std::string mask(std::string_view input) const override;
};

/// PhoneMask: masks all but last 4 digits, preserving separators.
/// Example: "+1-555-123-4567" -> "xxx-xxx-xxx-4567"
class PhoneMask final : public MaskingFunction {
 public:
  std::string mask(std::string_view input) const override;
};

}  // namespace arangodb::maskings

#endif  // ARANGODB_ATTRIBUTE_MASKING_H
