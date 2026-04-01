#include "Enterprise/Replication/TlsConnectionConfig.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

// ============================================================
// Happy path: all fields set correctly
// ============================================================

TEST(TlsConnectionConfigTest, ValidConfig_AllFieldsSet) {
  arangodb::TlsConnectionConfig cfg(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt",
      "1.3",
      "TLS_AES_256_GCM_SHA384");

  EXPECT_EQ(cfg.targetUrl(), "https://dc2.example.com:8529");
  EXPECT_EQ(cfg.clientCertPath(), "/etc/ssl/client.crt");
  EXPECT_EQ(cfg.clientKeyPath(), "/etc/ssl/client.key");
  EXPECT_EQ(cfg.caPath(), "/etc/ssl/ca.crt");
  EXPECT_EQ(cfg.minTlsVersion(), "1.3");
  EXPECT_EQ(cfg.cipherSuites(), "TLS_AES_256_GCM_SHA384");
}

// ============================================================
// Default TLS version is 1.2
// ============================================================

TEST(TlsConnectionConfigTest, DefaultTlsVersion_Is12) {
  arangodb::TlsConnectionConfig cfg(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt");

  EXPECT_EQ(cfg.minTlsVersion(), "1.2");
}

// ============================================================
// Default cipher suites is empty
// ============================================================

TEST(TlsConnectionConfigTest, DefaultCipherSuites_IsEmpty) {
  arangodb::TlsConnectionConfig cfg(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt");

  EXPECT_TRUE(cfg.cipherSuites().empty());
}

// ============================================================
// Validation: empty target URL throws
// ============================================================

TEST(TlsConnectionConfigTest, EmptyTargetUrl_Throws) {
  EXPECT_THROW(
      {
        arangodb::TlsConnectionConfig cfg(
            "",
            "/etc/ssl/client.crt",
            "/etc/ssl/client.key",
            "/etc/ssl/ca.crt");
      },
      std::invalid_argument);
}

// ============================================================
// Validation: empty client cert path throws
// ============================================================

TEST(TlsConnectionConfigTest, EmptyCertPath_Throws) {
  EXPECT_THROW(
      {
        arangodb::TlsConnectionConfig cfg(
            "https://dc2.example.com:8529",
            "",
            "/etc/ssl/client.key",
            "/etc/ssl/ca.crt");
      },
      std::invalid_argument);
}

// ============================================================
// Validation: empty CA path throws
// ============================================================

TEST(TlsConnectionConfigTest, EmptyCaPath_Throws) {
  EXPECT_THROW(
      {
        arangodb::TlsConnectionConfig cfg(
            "https://dc2.example.com:8529",
            "/etc/ssl/client.crt",
            "/etc/ssl/client.key",
            "");
      },
      std::invalid_argument);
}

// ============================================================
// Validation: invalid TLS version throws
// ============================================================

TEST(TlsConnectionConfigTest, InvalidTlsVersion_Throws) {
  EXPECT_THROW(
      {
        arangodb::TlsConnectionConfig cfg(
            "https://dc2.example.com:8529",
            "/etc/ssl/client.crt",
            "/etc/ssl/client.key",
            "/etc/ssl/ca.crt",
            "1.0");
      },
      std::invalid_argument);
}

// ============================================================
// Validation: TLS 1.1 is rejected
// ============================================================

TEST(TlsConnectionConfigTest, Tls11_Rejected) {
  EXPECT_THROW(
      {
        arangodb::TlsConnectionConfig cfg(
            "https://dc2.example.com:8529",
            "/etc/ssl/client.crt",
            "/etc/ssl/client.key",
            "/etc/ssl/ca.crt",
            "1.1");
      },
      std::invalid_argument);
}

// ============================================================
// Config is effectively immutable (no setters, only const accessors)
// ============================================================

TEST(TlsConnectionConfigTest, Immutability_CopyConstruct) {
  arangodb::TlsConnectionConfig original(
      "https://dc2.example.com:8529",
      "/etc/ssl/client.crt",
      "/etc/ssl/client.key",
      "/etc/ssl/ca.crt",
      "1.2",
      "ECDHE-RSA-AES256-GCM-SHA384");

  arangodb::TlsConnectionConfig copy(original);
  EXPECT_EQ(copy.targetUrl(), original.targetUrl());
  EXPECT_EQ(copy.clientCertPath(), original.clientCertPath());
  EXPECT_EQ(copy.clientKeyPath(), original.clientKeyPath());
  EXPECT_EQ(copy.caPath(), original.caPath());
  EXPECT_EQ(copy.minTlsVersion(), original.minTlsVersion());
  EXPECT_EQ(copy.cipherSuites(), original.cipherSuites());
}
