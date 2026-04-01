#pragma once
#ifndef ARANGODB_DIRECTMQ_PROTOCOL_H
#define ARANGODB_DIRECTMQ_PROTOCOL_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {

/// Wire protocol for DirectMQ inter-datacenter messaging.
///
/// Message frame format (big-endian):
///   [4 bytes: payload length] [N bytes: payload]
///
/// ACK frame format (big-endian):
///   [4 bytes: payload length = 4] [4 bytes: status code]
///   Status 0 = success, non-zero = error code.
///
/// Maximum payload size: 64 MB (to prevent memory exhaustion from
/// malformed length prefixes).
class DirectMQProtocol {
 public:
  /// Maximum allowed payload size (64 MB).
  static constexpr uint32_t kMaxPayloadSize = 64 * 1024 * 1024;

  /// ACK status codes.
  static constexpr uint32_t kAckSuccess = 0;
  static constexpr uint32_t kAckErrorGeneric = 1;
  static constexpr uint32_t kAckErrorSequence = 2;  // sequence number rejected

  /// Result of a parse operation.
  struct ParseResult {
    bool ok = false;
    std::string payload;       // extracted payload (for messages)
    uint32_t statusCode = 0;   // extracted status (for ACKs)
    std::string errorMessage;  // human-readable error on failure
  };

  /// Frame a payload into a length-prefixed message.
  /// Throws std::invalid_argument if payload exceeds kMaxPayloadSize.
  /// Returns the framed bytes (4-byte header + payload).
  static std::vector<uint8_t> frameMessage(std::string_view payload);

  /// Parse a length-prefixed message frame from a buffer.
  /// The buffer must contain at least 4 bytes (header) + declared length.
  /// Returns ParseResult with ok=true and payload on success.
  static ParseResult parseFrame(std::vector<uint8_t> const& buffer);

  /// Build an ACK response frame (8 bytes total).
  static std::vector<uint8_t> buildAck(uint32_t statusCode);

  /// Parse an ACK response from a buffer (expects exactly 8 bytes).
  static ParseResult parseAck(std::vector<uint8_t> const& buffer);
};

}  // namespace arangodb

#endif  // ARANGODB_DIRECTMQ_PROTOCOL_H
