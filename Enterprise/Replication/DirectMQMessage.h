#pragma once
#ifndef ARANGODB_DIRECT_MQ_MESSAGE_H
#define ARANGODB_DIRECT_MQ_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

/// Operation type carried in a DC2DC replication message.
enum class Operation : uint8_t {
  Insert = 0,
  Update = 1,
  Delete = 2
};

/// A single replication message in the DirectMQ protocol.
/// Each message is tagged with a per-shard monotonic sequence number
/// that enables idempotent replay on the target datacenter.
struct DirectMQMessage {
  std::string shard_id;                        // Source shard identifier
  uint64_t sequence = 0;                       // Monotonic sequence within shard
  Operation operation = Operation::Insert;     // Write operation type
  std::vector<uint8_t> payload;                // VPack-encoded document bytes

  DirectMQMessage() = default;

  DirectMQMessage(std::string shard, uint64_t seq, Operation op,
                  std::vector<uint8_t> data)
      : shard_id(std::move(shard)),
        sequence(seq),
        operation(op),
        payload(std::move(data)) {}
};

}  // namespace arangodb

#endif  // ARANGODB_DIRECT_MQ_MESSAGE_H
