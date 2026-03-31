#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace arangodb::test {

struct MockShardInfo {
  uint32_t numberOfShards;
  std::vector<std::string> shardNames;

  static MockShardInfo create(uint32_t n) {
    MockShardInfo info{n, {}};
    for (uint32_t i = 0; i < n; ++i) {
      info.shardNames.push_back("s" + std::to_string(i));
    }
    return info;
  }
};

}  // namespace arangodb::test
