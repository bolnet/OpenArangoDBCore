#pragma once
#ifndef ARANGODB_KEY_SPACE_PARTITIONER_H
#define ARANGODB_KEY_SPACE_PARTITIONER_H

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct KeyRange {
  std::string lowerBound;  // inclusive
  std::string upperBound;  // exclusive (empty = unbounded)
};

class KeySpacePartitioner {
 public:
  /// Split the key space [lowerBound, upperBound) into numPartitions
  /// non-overlapping ranges. If upperBound is empty, it means unbounded.
  static std::vector<KeyRange> partition(
      std::string const& lowerBound,
      std::string const& upperBound,
      uint32_t numPartitions);

  /// Compute partition boundaries by sampling keys from an iterator.
  /// Returns numPartitions-1 split points.
  static std::vector<std::string> computeSplitPoints(
      std::vector<std::string> const& sampledKeys,
      uint32_t numPartitions);
};

}  // namespace arangodb
#endif
