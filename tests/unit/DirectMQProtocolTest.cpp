#include "Enterprise/Replication/DirectMQProtocol.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using arangodb::DirectMQProtocol;

// ============================================================
// frameMessage tests
// ============================================================

TEST(DirectMQProtocolTest, FrameMessage_LengthPrefixed) {
  auto frame = DirectMQProtocol::frameMessage("hello");

  // Expected: [0x00, 0x00, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o']
  ASSERT_EQ(frame.size(), 9u);

  // Big-endian length = 5
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x00);
  EXPECT_EQ(frame[3], 0x05);

  // Payload
  EXPECT_EQ(frame[4], 'h');
  EXPECT_EQ(frame[5], 'e');
  EXPECT_EQ(frame[6], 'l');
  EXPECT_EQ(frame[7], 'l');
  EXPECT_EQ(frame[8], 'o');
}

TEST(DirectMQProtocolTest, FrameEmptyMessage_ZeroLength) {
  auto frame = DirectMQProtocol::frameMessage("");

  // Expected: [0x00, 0x00, 0x00, 0x00]
  ASSERT_EQ(frame.size(), 4u);
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x00);
  EXPECT_EQ(frame[3], 0x00);
}

TEST(DirectMQProtocolTest, FrameMessage_LargerPayload) {
  std::string payload(1024, 'X');
  auto frame = DirectMQProtocol::frameMessage(payload);

  ASSERT_EQ(frame.size(), 4u + 1024u);

  // Big-endian length = 1024 = 0x00000400
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x04);
  EXPECT_EQ(frame[3], 0x00);
}

// ============================================================
// parseFrame tests
// ============================================================

TEST(DirectMQProtocolTest, ParseFrame_ExtractsPayload) {
  // Frame "hello" then parse it back
  auto frame = DirectMQProtocol::frameMessage("hello");
  auto result = DirectMQProtocol::parseFrame(frame);

  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.payload, "hello");
}

TEST(DirectMQProtocolTest, ParseFrame_EmptyPayload) {
  auto frame = DirectMQProtocol::frameMessage("");
  auto result = DirectMQProtocol::parseFrame(frame);

  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.payload, "");
}

TEST(DirectMQProtocolTest, ParseFrame_InsufficientHeader_ReturnsError) {
  // Only 2 bytes -- need at least 4
  std::vector<uint8_t> buffer = {0x00, 0x05};
  auto result = DirectMQProtocol::parseFrame(buffer);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(DirectMQProtocolTest, ParseFrame_InsufficientPayload_ReturnsError) {
  // Header claims 10 bytes but buffer only has header + 2
  std::vector<uint8_t> buffer = {0x00, 0x00, 0x00, 0x0A, 0x41, 0x42};
  auto result = DirectMQProtocol::parseFrame(buffer);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(DirectMQProtocolTest, ParseFrame_EmptyBuffer_ReturnsError) {
  std::vector<uint8_t> buffer;
  auto result = DirectMQProtocol::parseFrame(buffer);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================
// Round-trip: frame then parse
// ============================================================

TEST(DirectMQProtocolTest, RoundTrip_PreservesPayload) {
  std::string original = "replication-data-{\"seq\":42,\"op\":\"insert\"}";
  auto frame = DirectMQProtocol::frameMessage(original);
  auto parsed = DirectMQProtocol::parseFrame(frame);

  ASSERT_TRUE(parsed.ok);
  EXPECT_EQ(parsed.payload, original);
}

// ============================================================
// ACK tests
// ============================================================

TEST(DirectMQProtocolTest, ParseAck_Success) {
  auto ack = DirectMQProtocol::buildAck(DirectMQProtocol::kAckSuccess);
  ASSERT_EQ(ack.size(), 8u);

  auto result = DirectMQProtocol::parseAck(ack);
  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.statusCode, DirectMQProtocol::kAckSuccess);
}

TEST(DirectMQProtocolTest, ParseAck_ErrorStatus) {
  auto ack = DirectMQProtocol::buildAck(DirectMQProtocol::kAckErrorGeneric);
  auto result = DirectMQProtocol::parseAck(ack);

  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.statusCode, DirectMQProtocol::kAckErrorGeneric);
}

TEST(DirectMQProtocolTest, ParseAck_SequenceError) {
  auto ack = DirectMQProtocol::buildAck(DirectMQProtocol::kAckErrorSequence);
  auto result = DirectMQProtocol::parseAck(ack);

  ASSERT_TRUE(result.ok);
  EXPECT_EQ(result.statusCode, DirectMQProtocol::kAckErrorSequence);
}

TEST(DirectMQProtocolTest, ParseAck_InvalidLength_ReturnsError) {
  // Only 4 bytes -- need 8
  std::vector<uint8_t> buffer = {0x00, 0x00, 0x00, 0x04};
  auto result = DirectMQProtocol::parseAck(buffer);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(DirectMQProtocolTest, ParseAck_WrongPayloadLength_ReturnsError) {
  // 8 bytes but first 4 claim payload length != 4
  std::vector<uint8_t> buffer = {0x00, 0x00, 0x00, 0x08,
                                  0x00, 0x00, 0x00, 0x00};
  auto result = DirectMQProtocol::parseAck(buffer);

  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================
// ACK format verification
// ============================================================

TEST(DirectMQProtocolTest, BuildAck_Format) {
  auto ack = DirectMQProtocol::buildAck(0);

  // [4-byte length = 4] [4-byte status = 0]
  ASSERT_EQ(ack.size(), 8u);
  EXPECT_EQ(ack[0], 0x00);
  EXPECT_EQ(ack[1], 0x00);
  EXPECT_EQ(ack[2], 0x00);
  EXPECT_EQ(ack[3], 0x04);  // payload length = 4
  EXPECT_EQ(ack[4], 0x00);
  EXPECT_EQ(ack[5], 0x00);
  EXPECT_EQ(ack[6], 0x00);
  EXPECT_EQ(ack[7], 0x00);  // status = 0
}

// ============================================================
// Max message size enforcement
// ============================================================

TEST(DirectMQProtocolTest, MaxMessageSize_Enforced) {
  // Create a payload just over the max
  // We cannot actually allocate 64MB+1 in a unit test reliably,
  // but we can test with a payload that the check catches.
  // The actual check is size > kMaxPayloadSize. We verify the
  // constant is correct and that the exception path works.
  EXPECT_EQ(DirectMQProtocol::kMaxPayloadSize, 64u * 1024u * 1024u);

  // Verify that a parse with declared length > max is rejected
  std::vector<uint8_t> buffer = {
      0x04, 0x00, 0x00, 0x01,  // length = 64MB + 1
      0x00                      // minimal payload byte
  };
  auto result = DirectMQProtocol::parseFrame(buffer);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}
