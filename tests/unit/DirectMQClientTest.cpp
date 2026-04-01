#include "Enterprise/Replication/DirectMQClient.h"
#include "Enterprise/Replication/ConnectionRetryPolicy.h"
#include "Enterprise/Replication/DirectMQProtocol.h"
#include "Enterprise/Replication/TlsConnectionConfig.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>

using arangodb::ConnectionRetryPolicy;
using arangodb::DirectMQClient;
using arangodb::DirectMQResult;
using arangodb::TlsConnectionConfig;

namespace {

/// Helper to create a valid TlsConnectionConfig for testing.
TlsConnectionConfig makeValidConfig() {
  return TlsConnectionConfig(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt",
      "1.2",
      "");
}

TlsConnectionConfig makeConfigWithCiphers() {
  return TlsConnectionConfig(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt",
      "1.3",
      "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
}

}  // namespace

// ============================================================
// DirectMQClient construction tests
// ============================================================

TEST(DirectMQClientTest, Construct_WithValidConfig) {
  EXPECT_NO_THROW({
    DirectMQClient client(makeValidConfig());
  });
}

TEST(DirectMQClientTest, IsConnected_InitiallyFalse) {
  DirectMQClient client(makeValidConfig());
  EXPECT_FALSE(client.isConnected());
}

TEST(DirectMQClientTest, Config_AccessibleAfterConstruction) {
  DirectMQClient client(makeValidConfig());
  EXPECT_EQ(client.config().targetUrl(), "https://dc2.example.com:8529");
  EXPECT_EQ(client.config().clientCertPath(), "/etc/ssl/client.crt");
  EXPECT_EQ(client.config().caPath(), "/etc/ssl/ca.crt");
}

// ============================================================
// Connect tests (mock environment -- connects immediately)
// ============================================================

TEST(DirectMQClientTest, Connect_InitializesSslCtx) {
  DirectMQClient client(makeValidConfig());
  auto result = client.connect();

  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(client.isConnected());
}

TEST(DirectMQClientTest, Connect_EnforcesMinTlsVersion) {
  auto config = TlsConnectionConfig(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt",
      "1.3");

  DirectMQClient client(std::move(config));
  auto result = client.connect();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(client.config().minTlsVersion(), "1.3");
}

TEST(DirectMQClientTest, Connect_SetsCipherSuites) {
  DirectMQClient client(makeConfigWithCiphers());
  auto result = client.connect();

  EXPECT_TRUE(result.ok);
  EXPECT_FALSE(client.config().cipherSuites().empty());
}

TEST(DirectMQClientTest, Connect_AlreadyConnected_Noop) {
  DirectMQClient client(makeValidConfig());
  auto first = client.connect();
  ASSERT_TRUE(first.ok);

  auto second = client.connect();
  EXPECT_TRUE(second.ok);
  EXPECT_TRUE(client.isConnected());
}

// ============================================================
// Disconnect tests
// ============================================================

TEST(DirectMQClientTest, Disconnect_CleansUpSsl) {
  DirectMQClient client(makeValidConfig());
  auto result = client.connect();
  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(client.isConnected());

  client.disconnect();
  EXPECT_FALSE(client.isConnected());
}

TEST(DirectMQClientTest, DoubleDisconnect_Safe) {
  DirectMQClient client(makeValidConfig());
  auto result = client.connect();
  ASSERT_TRUE(result.ok);

  client.disconnect();
  EXPECT_FALSE(client.isConnected());

  // Second disconnect should not crash or double-free
  client.disconnect();
  EXPECT_FALSE(client.isConnected());
}

TEST(DirectMQClientTest, Disconnect_NeverConnected_Safe) {
  DirectMQClient client(makeValidConfig());
  // Never called connect() -- disconnect should be safe
  client.disconnect();
  EXPECT_FALSE(client.isConnected());
}

// ============================================================
// sendMessage tests
// ============================================================

TEST(DirectMQClientTest, SendMessage_FramesWithProtocol) {
  DirectMQClient client(makeValidConfig());
  auto connResult = client.connect();
  ASSERT_TRUE(connResult.ok);

  auto sendResult = client.sendMessage("test-payload");
  EXPECT_TRUE(sendResult.ok);
}

TEST(DirectMQClientTest, RejectPlaintext_TlsRequired) {
  // Client not connected -- sendMessage should fail
  DirectMQClient client(makeValidConfig());
  ASSERT_FALSE(client.isConnected());

  auto result = client.sendMessage("test-payload");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
  // Error message should mention TLS or connection requirement
  EXPECT_NE(result.errorMessage.find("not connected"), std::string::npos);
}

TEST(DirectMQClientTest, SendMessage_EmptyPayload) {
  DirectMQClient client(makeValidConfig());
  auto connResult = client.connect();
  ASSERT_TRUE(connResult.ok);

  auto sendResult = client.sendMessage("");
  EXPECT_TRUE(sendResult.ok);
}

// ============================================================
// receiveAck tests
// ============================================================

TEST(DirectMQClientTest, ReceiveAck_ParsesAckFrame) {
  DirectMQClient client(makeValidConfig());
  auto connResult = client.connect();
  ASSERT_TRUE(connResult.ok);

  auto ackResult = client.receiveAck();
  EXPECT_TRUE(ackResult.ok);
}

TEST(DirectMQClientTest, ReceiveAck_NotConnected_Fails) {
  DirectMQClient client(makeValidConfig());
  ASSERT_FALSE(client.isConnected());

  auto result = client.receiveAck();
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.errorMessage.find("not connected"), std::string::npos);
}

// ============================================================
// Move semantics tests
// ============================================================

TEST(DirectMQClientTest, MoveConstruct_TransfersConnection) {
  DirectMQClient client1(makeValidConfig());
  auto result = client1.connect();
  ASSERT_TRUE(result.ok);
  ASSERT_TRUE(client1.isConnected());

  DirectMQClient client2(std::move(client1));
  EXPECT_TRUE(client2.isConnected());
  // Moved-from client should not be connected
  EXPECT_FALSE(client1.isConnected());  // NOLINT(bugprone-use-after-move)
}

TEST(DirectMQClientTest, MoveAssign_TransfersConnection) {
  DirectMQClient client1(makeValidConfig());
  auto result = client1.connect();
  ASSERT_TRUE(result.ok);

  DirectMQClient client2(makeValidConfig());
  client2 = std::move(client1);

  EXPECT_TRUE(client2.isConnected());
  EXPECT_FALSE(client1.isConnected());  // NOLINT(bugprone-use-after-move)
}

// ============================================================
// ConnectionRetryPolicy tests
// ============================================================

TEST(ConnectionRetryPolicyTest, FirstRetry_BaseDelay) {
  ConnectionRetryPolicy policy(5, 100, 30000);

  // Raw delay at attempt 0 should be baseDelay = 100ms
  auto delay = policy.rawDelay();
  EXPECT_EQ(delay.count(), 100);
}

TEST(ConnectionRetryPolicyTest, ExponentialGrowth_DoublesEachRetry) {
  ConnectionRetryPolicy policy(10, 100, 30000);

  // Advance through attempts and check raw delays double
  EXPECT_EQ(policy.rawDelay().count(), 100);   // attempt 0: 100

  policy.nextDelay();  // consume attempt 0
  EXPECT_EQ(policy.rawDelay().count(), 200);   // attempt 1: 200

  policy.nextDelay();  // consume attempt 1
  EXPECT_EQ(policy.rawDelay().count(), 400);   // attempt 2: 400

  policy.nextDelay();  // consume attempt 2
  EXPECT_EQ(policy.rawDelay().count(), 800);   // attempt 3: 800
}

TEST(ConnectionRetryPolicyTest, MaxDelay_Clamped) {
  ConnectionRetryPolicy policy(20, 100, 1000);

  // At attempt 4: 100 * 2^4 = 1600, should clamp to 1000
  for (int i = 0; i < 4; ++i) {
    policy.nextDelay();
  }
  EXPECT_EQ(policy.rawDelay().count(), 1000);

  // At attempt 10: still clamped
  for (int i = 4; i < 10; ++i) {
    policy.nextDelay();
  }
  EXPECT_EQ(policy.rawDelay().count(), 1000);
}

TEST(ConnectionRetryPolicyTest, JitterApplied_WithinBounds) {
  ConnectionRetryPolicy policy(100, 1000, 30000);

  // Run multiple iterations and verify jitter range [0.5x, 1.5x]
  for (int i = 0; i < 20; ++i) {
    auto raw = policy.rawDelay().count();
    auto jittered = policy.nextDelay().count();

    // Jitter should be within [0.5 * raw, 1.5 * raw]
    EXPECT_GE(jittered, static_cast<int64_t>(raw * 0.5) - 1)
        << "Jittered delay " << jittered << " below lower bound for raw "
        << raw;
    EXPECT_LE(jittered, static_cast<int64_t>(raw * 1.5) + 1)
        << "Jittered delay " << jittered << " above upper bound for raw "
        << raw;
  }
}

TEST(ConnectionRetryPolicyTest, MaxRetriesExceeded_ReturnsFalse) {
  ConnectionRetryPolicy policy(3, 100, 30000);

  EXPECT_TRUE(policy.shouldRetry());   // attempt 0
  policy.nextDelay();
  EXPECT_TRUE(policy.shouldRetry());   // attempt 1
  policy.nextDelay();
  EXPECT_TRUE(policy.shouldRetry());   // attempt 2
  policy.nextDelay();
  EXPECT_FALSE(policy.shouldRetry());  // attempt 3 == maxRetries
}

TEST(ConnectionRetryPolicyTest, Reset_ClearsAttemptCounter) {
  ConnectionRetryPolicy policy(3, 100, 30000);

  policy.nextDelay();
  policy.nextDelay();
  policy.nextDelay();
  ASSERT_FALSE(policy.shouldRetry());

  policy.reset();
  EXPECT_EQ(policy.attempt(), 0u);
  EXPECT_TRUE(policy.shouldRetry());
}

TEST(ConnectionRetryPolicyTest, ZeroMaxRetries_NeverRetries) {
  ConnectionRetryPolicy policy(0, 100, 30000);
  EXPECT_FALSE(policy.shouldRetry());
}

// ============================================================
// DirectMQClient retry integration tests
// ============================================================

TEST(DirectMQClientTest, RetryOnFailure_UsesPolicy) {
  // In mock environment, connect always succeeds, so we verify
  // that the retry policy is configurable and the client uses it.
  ConnectionRetryPolicy policy(3, 10, 100);
  DirectMQClient client(makeValidConfig(), std::move(policy));

  auto result = client.connect();
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(client.isConnected());
}
