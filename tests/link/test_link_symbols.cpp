// Compilation-only link test: verifies one symbol from each core module
// resolves from libopenarangodb_enterprise.a. If this compiles and links,
// static library link order is correct.

#include "Enterprise/License/LicenseFeature.h"
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#include "Enterprise/RClone/RCloneFeature.h"

#include <gtest/gtest.h>

namespace {
arangodb::application_features::ApplicationServer g_server;
}

TEST(LinkSymbolsTest, LicenseFeatureSymbolLinksCorrectly) {
  arangodb::LicenseFeature f(g_server);
  EXPECT_EQ(f.name(), "License");
}

TEST(LinkSymbolsTest, AuditFeatureSymbolLinksCorrectly) {
  arangodb::AuditFeature f(g_server);
  EXPECT_EQ(f.name(), "Audit");
}

TEST(LinkSymbolsTest, EncryptionFeatureSymbolLinksCorrectly) {
  arangodb::EncryptionFeature f(g_server);
  EXPECT_EQ(f.name(), "Encryption");
}

TEST(LinkSymbolsTest, RCloneFeatureSymbolLinksCorrectly) {
  arangodb::RCloneFeature f(g_server);
  EXPECT_EQ(f.name(), "RClone");
}
