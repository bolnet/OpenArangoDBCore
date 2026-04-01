#include "ReplicationCheckpoint.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace arangodb {

ReplicationCheckpoint::ReplicationCheckpoint(
    std::string path, SequenceNumberTracker& tracker)
    : _path(std::move(path)), _tracker(tracker) {}

CheckpointResult ReplicationCheckpoint::save() const {
  auto state = _tracker.getState();

  // Build JSON string.
  std::ostringstream oss;
  oss << "{\n  \"version\": 1,\n  \"shards\": {";
  bool first = true;
  for (auto const& [shardId, seq] : state) {
    if (!first) {
      oss << ",";
    }
    oss << "\n    \"" << shardId << "\": " << seq;
    first = false;
  }
  oss << "\n  }\n}\n";

  // Write to temporary file then rename for atomicity.
  std::string tmpPath = _path + ".tmp";
  {
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open()) {
      return {false, "Failed to open temporary checkpoint file: " + tmpPath};
    }
    out << oss.str();
    out.flush();
    if (!out.good()) {
      return {false, "Failed to write checkpoint data to: " + tmpPath};
    }
  }

  std::error_code ec;
  std::filesystem::rename(tmpPath, _path, ec);
  if (ec) {
    return {false, "Failed to rename checkpoint file: " + ec.message()};
  }

  return {true, ""};
}

CheckpointResult ReplicationCheckpoint::load() {
  if (!std::filesystem::exists(_path)) {
    // Fresh start -- no checkpoint to restore.
    return {true, ""};
  }

  std::ifstream in(_path);
  if (!in.is_open()) {
    return {false, "Failed to open checkpoint file: " + _path};
  }

  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  // Parse JSON manually (minimal dependency approach).
  // Expected format: {"version":1,"shards":{"shard_id":seq,...}}
  std::unordered_map<std::string, uint64_t> state;

  // Find "shards" object.
  auto shardsPos = content.find("\"shards\"");
  if (shardsPos == std::string::npos) {
    return {false, "Checkpoint file missing 'shards' key"};
  }

  // Find the opening brace of the shards object.
  auto bracePos = content.find('{', shardsPos);
  if (bracePos == std::string::npos) {
    return {false, "Checkpoint file has malformed 'shards' object"};
  }

  // Parse shard entries: "shard_id": number
  size_t pos = bracePos + 1;
  while (pos < content.size()) {
    // Find next quoted key.
    auto quoteStart = content.find('"', pos);
    if (quoteStart == std::string::npos) {
      break;
    }
    auto quoteEnd = content.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) {
      return {false, "Checkpoint file has unterminated string"};
    }

    std::string key = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

    // Find colon after key.
    auto colonPos = content.find(':', quoteEnd);
    if (colonPos == std::string::npos) {
      break;
    }

    // Check we haven't passed the closing brace of shards.
    auto closeBrace = content.find('}', quoteEnd);
    if (closeBrace != std::string::npos && closeBrace < colonPos) {
      break;
    }

    // Parse the number value.
    size_t numStart = colonPos + 1;
    while (numStart < content.size() && (content[numStart] == ' ' ||
           content[numStart] == '\n' || content[numStart] == '\r' ||
           content[numStart] == '\t')) {
      ++numStart;
    }

    size_t numEnd = numStart;
    while (numEnd < content.size() && content[numEnd] >= '0' &&
           content[numEnd] <= '9') {
      ++numEnd;
    }

    if (numEnd == numStart) {
      return {false, "Checkpoint file has non-numeric sequence for shard: " + key};
    }

    uint64_t seq = 0;
    try {
      seq = std::stoull(content.substr(numStart, numEnd - numStart));
    } catch (...) {
      return {false, "Checkpoint file has invalid sequence number for shard: " + key};
    }

    state[key] = seq;
    pos = numEnd;
  }

  _tracker.restoreState(state);
  return {true, ""};
}

}  // namespace arangodb
