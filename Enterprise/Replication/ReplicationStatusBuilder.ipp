#pragma once

#include <algorithm>
#include <sstream>

namespace arangodb {

template <typename TrackerT, typename TailerT>
std::string ReplicationStatusBuilderT<TrackerT, TailerT>::build() const {
  auto states = _tracker.getAllStates();
  uint64_t totalPending = _tracker.totalPending();
  uint64_t sent = _tailer.messagesSent();
  uint64_t acked = _tailer.messagesAcked();
  bool running = _tailer.isRunning();

  // Find max pending for lag estimation
  uint64_t maxPending = 0;
  for (auto const& s : states) {
    maxPending = std::max(maxPending, s.pendingCount);
  }

  double lag = estimateLagSeconds(maxPending, sent, acked);

  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"isRunning\": " << (running ? "true" : "false") << ",\n";
  oss << "  \"lagSeconds\": " << lag << ",\n";
  oss << "  \"totalMessagesSent\": " << sent << ",\n";
  oss << "  \"totalMessagesAcked\": " << acked << ",\n";
  oss << "  \"totalPending\": " << totalPending << ",\n";
  oss << "  \"shardStates\": [";

  for (size_t i = 0; i < states.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    auto const& s = states[i];
    oss << "{\n";
    oss << "    \"shardId\": \"" << s.shardId << "\",\n";
    oss << "    \"lastGenerated\": " << s.lastGenerated << ",\n";
    oss << "    \"lastApplied\": " << s.lastApplied << ",\n";
    oss << "    \"pending\": " << s.pendingCount << "\n";
    oss << "  }";
  }

  oss << "]\n";
  oss << "}";

  return oss.str();
}

template <typename TrackerT, typename TailerT>
double ReplicationStatusBuilderT<TrackerT, TailerT>::estimateLagSeconds(
    uint64_t maxPending, uint64_t messagesSent, uint64_t messagesAcked) {
  if (maxPending == 0) {
    return 0.0;
  }
  // Estimate throughput from acked/sent ratio. If no messages sent yet,
  // assume worst case of 1 msg/sec throughput.
  if (messagesAcked == 0 || messagesSent == 0) {
    return static_cast<double>(maxPending);
  }
  // Throughput = acked messages / (sent messages * assumed_interval)
  // Simplified: lag = maxPending * (sent / acked) as a ratio proxy
  double ratio = static_cast<double>(messagesSent) /
                 static_cast<double>(messagesAcked);
  return static_cast<double>(maxPending) * ratio;
}

}  // namespace arangodb
