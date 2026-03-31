#pragma once
/// Mock SSL types for standalone testing of SslServerFeatureEE.
/// These provide enough interface to verify option parsing, validation,
/// and configuration logic without linking to real OpenSSL.

#include <string>
#include <vector>

namespace arangodb::test {

/// Tracks SSL_CTX configuration calls for test verification.
struct MockSslContext {
  bool verifyPeerEnabled = false;
  bool verifyFailIfNoPeerCert = false;
  std::string cipherList;
  std::string cipherSuites;  // TLS 1.3
  int minProtoVersion = 0;
  std::string caFile;

  // SSL_VERIFY flags (matching OpenSSL constants)
  static constexpr int SSL_VERIFY_NONE = 0x00;
  static constexpr int SSL_VERIFY_PEER = 0x01;
  static constexpr int SSL_VERIFY_FAIL_IF_NO_PEER_CERT = 0x02;
};

}  // namespace arangodb::test
