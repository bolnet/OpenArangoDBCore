#pragma once
#ifndef ARANGODB_SMART_GRAPH_SCHEMA_H
#define ARANGODB_SMART_GRAPH_SCHEMA_H

#include <string>
#include <string_view>
#include <unordered_set>

namespace arangodb {

struct SmartGraphValidationResult {
  bool ok;
  std::string errorMessage;

  static SmartGraphValidationResult success() { return {true, ""}; }
  static SmartGraphValidationResult error(std::string msg) {
    return {false, std::move(msg)};
  }
  explicit operator bool() const { return ok; }
};

class SmartGraphSchema {
 public:
  /// Extract smart prefix from a _key value (everything before first ':').
  /// Returns empty string_view if no ':' is found.
  static std::string_view extractSmartValue(std::string_view key);

  /// Build a valid smart vertex _key from smart value and local key.
  static std::string buildSmartKey(std::string_view smartValue,
                                   std::string_view localKey);

  /// Build a valid smart edge _key.
  static std::string buildSmartEdgeKey(std::string_view fromSmart,
                                       std::string_view toSmart,
                                       std::string_view edgeKey);

  /// Validate a vertex document for SmartGraph constraints.
  /// - _key must contain ':' separator
  /// - prefix of _key must match the smartGraphAttribute value
  /// - smartGraphAttribute field must be present (non-empty)
  static SmartGraphValidationResult validateDocument(
      std::string_view key, std::string_view smartAttributeValue,
      std::string const& smartAttributeName);

  /// Validate an edge document for SmartGraph constraints.
  /// - If isDisjoint: _from and _to must have the same smart prefix
  /// - If satelliteCollections contains the collection name from _from/_to,
  ///   skip the smart check for that side
  static SmartGraphValidationResult validateEdge(
      std::string_view fromKey, std::string_view toKey, bool isDisjoint,
      std::unordered_set<std::string> const& satelliteCollections = {});

  /// Smart attribute can never be modified after creation.
  static bool canModifySmartGraphAttribute() { return false; }
};

}  // namespace arangodb

#endif  // ARANGODB_SMART_GRAPH_SCHEMA_H
