#include "DirectMQProtocol.h"

#include <cstring>
#include <stdexcept>

namespace arangodb {

namespace {

/// Write a uint32 in big-endian to a byte buffer at the given offset.
void writeBE32(std::vector<uint8_t>& buf, size_t offset, uint32_t value) {
  buf[offset + 0] = static_cast<uint8_t>((value >> 24) & 0xFF);
  buf[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
  buf[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
  buf[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

/// Read a uint32 in big-endian from a byte buffer at the given offset.
uint32_t readBE32(std::vector<uint8_t> const& buf, size_t offset) {
  return (static_cast<uint32_t>(buf[offset + 0]) << 24) |
         (static_cast<uint32_t>(buf[offset + 1]) << 16) |
         (static_cast<uint32_t>(buf[offset + 2]) << 8) |
         static_cast<uint32_t>(buf[offset + 3]);
}

}  // namespace

std::vector<uint8_t> DirectMQProtocol::frameMessage(std::string_view payload) {
  if (payload.size() > kMaxPayloadSize) {
    throw std::invalid_argument(
        "DirectMQProtocol::frameMessage: payload exceeds maximum size of " +
        std::to_string(kMaxPayloadSize) + " bytes");
  }

  auto length = static_cast<uint32_t>(payload.size());
  std::vector<uint8_t> frame(4 + length);
  writeBE32(frame, 0, length);
  if (length > 0) {
    std::memcpy(frame.data() + 4, payload.data(), length);
  }
  return frame;
}

DirectMQProtocol::ParseResult DirectMQProtocol::parseFrame(
    std::vector<uint8_t> const& buffer) {
  ParseResult result;

  if (buffer.size() < 4) {
    result.errorMessage =
        "Buffer too short for frame header (need 4 bytes, got " +
        std::to_string(buffer.size()) + ")";
    return result;
  }

  uint32_t length = readBE32(buffer, 0);

  if (length > kMaxPayloadSize) {
    result.errorMessage = "Declared payload length " +
                          std::to_string(length) + " exceeds maximum " +
                          std::to_string(kMaxPayloadSize);
    return result;
  }

  if (buffer.size() < 4 + length) {
    result.errorMessage =
        "Buffer too short for payload (need " +
        std::to_string(4 + length) + " bytes, got " +
        std::to_string(buffer.size()) + ")";
    return result;
  }

  result.ok = true;
  result.payload.assign(reinterpret_cast<char const*>(buffer.data() + 4),
                        length);
  return result;
}

std::vector<uint8_t> DirectMQProtocol::buildAck(uint32_t statusCode) {
  std::vector<uint8_t> ack(8);
  writeBE32(ack, 0, 4);           // payload length = 4 bytes
  writeBE32(ack, 4, statusCode);  // status code
  return ack;
}

DirectMQProtocol::ParseResult DirectMQProtocol::parseAck(
    std::vector<uint8_t> const& buffer) {
  ParseResult result;

  if (buffer.size() < 8) {
    result.errorMessage = "ACK buffer too short (need 8 bytes, got " +
                          std::to_string(buffer.size()) + ")";
    return result;
  }

  uint32_t payloadLen = readBE32(buffer, 0);
  if (payloadLen != 4) {
    result.errorMessage =
        "ACK payload length must be 4, got " + std::to_string(payloadLen);
    return result;
  }

  result.ok = true;
  result.statusCode = readBE32(buffer, 4);
  return result;
}

}  // namespace arangodb
