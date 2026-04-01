#pragma once

#include <sstream>

namespace arangodb {

template <typename TailerT>
std::string ReplicationFeedStreamT<TailerT>::fetch(
    std::string const& shardId, uint64_t afterSequence,
    uint32_t maxCount) const {
  // Fetch one extra to detect hasMore
  auto messages = _tailer.nextBatch(shardId, afterSequence, maxCount + 1);

  bool hasMore = messages.size() > maxCount;
  if (hasMore) {
    messages.resize(maxCount);  // trim to requested size
  }

  uint64_t lastSequence = afterSequence;

  std::ostringstream oss;
  oss << "{\n  \"messages\": [";

  for (size_t i = 0; i < messages.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    auto const& m = messages[i];
    oss << "{\n";
    oss << "    \"shardId\": \"" << m.shardId << "\",\n";
    oss << "    \"sequence\": " << m.sequence << ",\n";
    oss << "    \"operation\": \"" << m.operation << "\",\n";
    oss << "    \"payload\": \"" << m.payload << "\"\n";
    oss << "  }";
    lastSequence = m.sequence;
  }

  oss << "],\n";
  oss << "  \"hasMore\": " << (hasMore ? "true" : "false") << ",\n";
  oss << "  \"lastSequence\": " << lastSequence << "\n";
  oss << "}";

  return oss.str();
}

}  // namespace arangodb
