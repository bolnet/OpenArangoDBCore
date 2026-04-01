#pragma once
#ifndef ARANGODB_DIRECTMQ_CLIENT_H
#define ARANGODB_DIRECTMQ_CLIENT_H

#include "Enterprise/Replication/TlsConnectionConfig.h"
#include "Enterprise/Replication/ConnectionRetryPolicy.h"
#include "Enterprise/Replication/DirectMQProtocol.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Forward-declare OpenSSL types (real or mock)
struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;
struct ssl_st;
typedef struct ssl_st SSL;

namespace arangodb {

/// Result of a DirectMQClient operation.
struct DirectMQResult {
  bool ok = false;
  uint32_t ackStatus = 0;       // ACK status code (0 = success)
  std::string errorMessage;
};

/// mTLS-secured messaging client for DC-to-DC replication.
///
/// Establishes an outbound TLS connection to the target datacenter,
/// sends length-prefixed replication messages, and receives ACK responses.
/// Plaintext connections are never permitted -- TLS is required from the
/// first byte.
///
/// Lifecycle:
///   1. Construct with TlsConnectionConfig
///   2. connect() -- establishes TCP + TLS handshake
///   3. sendMessage() / receiveAck() -- exchange replication data
///   4. disconnect() -- clean shutdown
///
/// Thread safety: NOT thread-safe. Each replication thread should own
/// its own DirectMQClient instance.
class DirectMQClient {
 public:
  /// Construct a client with the given TLS configuration and retry policy.
  explicit DirectMQClient(
      TlsConnectionConfig config,
      ConnectionRetryPolicy retryPolicy = ConnectionRetryPolicy());

  /// Destructor calls disconnect() if still connected.
  ~DirectMQClient();

  // Non-copyable, movable
  DirectMQClient(DirectMQClient const&) = delete;
  DirectMQClient& operator=(DirectMQClient const&) = delete;
  DirectMQClient(DirectMQClient&&) noexcept;
  DirectMQClient& operator=(DirectMQClient&&) noexcept;

  /// Establish TCP connection and perform TLS handshake with mTLS.
  /// On failure, retries according to the ConnectionRetryPolicy.
  /// Returns error if all retry attempts exhausted.
  DirectMQResult connect();

  /// Send a replication message (framed via DirectMQProtocol).
  /// Requires an active TLS connection (isConnected() == true).
  DirectMQResult sendMessage(std::string_view payload);

  /// Receive an ACK response from the target (8-byte ACK frame).
  /// Blocks until ACK is received or timeout expires.
  DirectMQResult receiveAck();

  /// Gracefully disconnect: SSL_shutdown + close socket.
  /// Safe to call multiple times (idempotent).
  void disconnect();

  /// Whether the client has an active TLS connection.
  bool isConnected() const noexcept;

  /// Access the underlying config (read-only).
  TlsConnectionConfig const& config() const noexcept { return _config; }

 private:
  /// Initialize SSL_CTX with client cert, CA, TLS version, ciphers.
  DirectMQResult initializeSslContext();

  /// Perform the TCP connect + TLS handshake sequence.
  DirectMQResult performConnect();

  TlsConnectionConfig _config;
  ConnectionRetryPolicy _retryPolicy;
  SSL_CTX* _sslCtx = nullptr;
  SSL* _ssl = nullptr;
  int _socketFd = -1;
  bool _connected = false;
};

}  // namespace arangodb

#endif  // ARANGODB_DIRECTMQ_CLIENT_H
