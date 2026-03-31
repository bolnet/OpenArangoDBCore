#include "Enterprise/VocBase/SmartGraphSchema.h"

namespace arangodb {

std::string_view SmartGraphSchema::extractSmartValue(std::string_view key) {
  auto pos = key.find(':');
  if (pos == std::string_view::npos) {
    return {};
  }
  return key.substr(0, pos);
}

std::string SmartGraphSchema::buildSmartKey(std::string_view smartValue,
                                            std::string_view localKey) {
  std::string result;
  result.reserve(smartValue.size() + 1 + localKey.size());
  result.append(smartValue);
  result.push_back(':');
  result.append(localKey);
  return result;
}

std::string SmartGraphSchema::buildSmartEdgeKey(std::string_view fromSmart,
                                                std::string_view toSmart,
                                                std::string_view edgeKey) {
  std::string result;
  result.reserve(fromSmart.size() + 1 + toSmart.size() + 1 + edgeKey.size());
  result.append(fromSmart);
  result.push_back(':');
  result.append(toSmart);
  result.push_back(':');
  result.append(edgeKey);
  return result;
}

namespace {

/// Parse collection name from a document handle "collectionName/key".
/// Returns the collection name portion.
std::string_view parseCollectionName(std::string_view handle) {
  auto pos = handle.find('/');
  if (pos == std::string_view::npos) {
    return {};
  }
  return handle.substr(0, pos);
}

/// Extract the smart prefix from a document handle or plain key.
/// For handles like "collection/smartPrefix:localKey", extracts from the key
/// part. For plain keys like "smartPrefix:localKey", extracts directly.
std::string_view extractPrefixFromKeyOrHandle(std::string_view value) {
  auto slashPos = value.find('/');
  std::string_view key =
      (slashPos != std::string_view::npos) ? value.substr(slashPos + 1) : value;
  return SmartGraphSchema::extractSmartValue(key);
}

}  // namespace

SmartGraphValidationResult SmartGraphSchema::validateDocument(
    std::string_view key, std::string_view smartAttributeValue,
    std::string const& smartAttributeName) {
  if (smartAttributeValue.empty()) {
    return SmartGraphValidationResult::error(
        "missing smartGraphAttribute '" + smartAttributeName + "'");
  }

  auto colonPos = key.find(':');
  if (colonPos == std::string_view::npos) {
    return SmartGraphValidationResult::error(
        "key '" + std::string(key) +
        "' does not contain ':' separator required for SmartGraph");
  }

  auto prefix = key.substr(0, colonPos);
  if (prefix != smartAttributeValue) {
    return SmartGraphValidationResult::error(
        "key prefix '" + std::string(prefix) +
        "' does not match smartGraphAttribute value '" +
        std::string(smartAttributeValue) + "'");
  }

  return SmartGraphValidationResult::success();
}

SmartGraphValidationResult SmartGraphSchema::validateEdge(
    std::string_view fromKey, std::string_view toKey, bool isDisjoint,
    std::unordered_set<std::string> const& satelliteCollections) {
  if (!isDisjoint) {
    return SmartGraphValidationResult::success();
  }

  // Check if either side is a satellite collection
  auto fromColl = parseCollectionName(fromKey);
  auto toColl = parseCollectionName(toKey);

  bool fromIsSatellite =
      !fromColl.empty() &&
      satelliteCollections.count(std::string(fromColl)) > 0;
  bool toIsSatellite =
      !toColl.empty() && satelliteCollections.count(std::string(toColl)) > 0;

  // If either side is satellite, skip disjoint check for that side
  if (fromIsSatellite || toIsSatellite) {
    return SmartGraphValidationResult::success();
  }

  auto fromPrefix = extractPrefixFromKeyOrHandle(fromKey);
  auto toPrefix = extractPrefixFromKeyOrHandle(toKey);

  if (fromPrefix != toPrefix) {
    return SmartGraphValidationResult::error(
        "disjoint SmartGraph edge crosses partitions: from prefix '" +
        std::string(fromPrefix) + "' != to prefix '" +
        std::string(toPrefix) + "'");
  }

  return SmartGraphValidationResult::success();
}

}  // namespace arangodb
