#include "KeySpacePartitioner.h"

#include <algorithm>
#include <cassert>

namespace arangodb {

namespace {

/// Interpolate between two byte strings at a given fraction (0.0 to 1.0).
/// Produces a key that is approximately fraction of the way between lower
/// and upper in byte-order space.
std::string interpolateKey(std::string const& lower, std::string const& upper,
                           double fraction) {
  if (lower.empty() && upper.empty()) {
    return {};
  }

  // Determine the working length (max of both)
  size_t maxLen = std::max(lower.size(), upper.size());
  if (maxLen == 0) {
    return {};
  }

  // Pad both strings conceptually to maxLen with 0x00
  std::string result;
  result.reserve(maxLen);

  // Process from most significant byte to least significant
  // We build the result in reverse, then flip
  std::vector<uint8_t> bytes(maxLen, 0);
  for (size_t i = 0; i < maxLen; ++i) {
    uint8_t lo = (i < lower.size()) ? static_cast<uint8_t>(lower[i]) : 0;
    uint8_t hi = (i < upper.size()) ? static_cast<uint8_t>(upper[i]) : 0;
    double val = lo + (static_cast<double>(hi) - lo) * fraction;
    bytes[i] = static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
  }

  result.assign(bytes.begin(), bytes.end());

  // Trim trailing zeros for cleanliness, but keep at least one byte
  while (result.size() > 1 && result.back() == '\0') {
    result.pop_back();
  }

  return result;
}

}  // namespace

std::vector<KeyRange> KeySpacePartitioner::partition(
    std::string const& lowerBound,
    std::string const& upperBound,
    uint32_t numPartitions) {
  if (numPartitions == 0) {
    return {};
  }

  // Empty range check: lower >= upper (when upper is non-empty)
  if (!upperBound.empty() && lowerBound >= upperBound) {
    return {};
  }

  if (numPartitions == 1) {
    return {{lowerBound, upperBound}};
  }

  // Compute split points
  std::vector<std::string> splits;
  splits.reserve(numPartitions - 1);

  for (uint32_t i = 1; i < numPartitions; ++i) {
    double fraction = static_cast<double>(i) / static_cast<double>(numPartitions);
    if (upperBound.empty()) {
      // Unbounded upper: we can't interpolate meaningfully without samples.
      // Use a heuristic: append increasing byte prefixes to lowerBound.
      std::string splitKey = lowerBound;
      // Add a byte that represents the fraction of 0xFF
      splitKey.push_back(static_cast<char>(static_cast<uint8_t>(fraction * 255.0)));
      splits.push_back(std::move(splitKey));
    } else {
      splits.push_back(interpolateKey(lowerBound, upperBound, fraction));
    }
  }

  // Build partition ranges
  std::vector<KeyRange> result;
  result.reserve(numPartitions);

  std::string prevBound = lowerBound;
  for (auto const& split : splits) {
    result.push_back({prevBound, split});
    prevBound = split;
  }
  // Last partition goes to upperBound
  result.push_back({prevBound, upperBound});

  return result;
}

std::vector<std::string> KeySpacePartitioner::computeSplitPoints(
    std::vector<std::string> const& sampledKeys,
    uint32_t numPartitions) {
  if (numPartitions <= 1 || sampledKeys.empty()) {
    return {};
  }

  // Sort a copy of sampled keys
  std::vector<std::string> sorted = sampledKeys;
  std::sort(sorted.begin(), sorted.end());

  // Remove duplicates
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

  if (sorted.size() < numPartitions) {
    // Not enough unique keys for the requested partitions;
    // return all unique keys as split points
    return sorted;
  }

  std::vector<std::string> splitPoints;
  splitPoints.reserve(numPartitions - 1);

  for (uint32_t i = 1; i < numPartitions; ++i) {
    size_t idx = static_cast<size_t>(
        static_cast<double>(i) / numPartitions * sorted.size());
    if (idx >= sorted.size()) {
      idx = sorted.size() - 1;
    }
    splitPoints.push_back(sorted[idx]);
  }

  return splitPoints;
}

}  // namespace arangodb
