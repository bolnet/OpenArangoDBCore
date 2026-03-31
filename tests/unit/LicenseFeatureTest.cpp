#include "Enterprise/License/LicenseFeature.h"
#include <gtest/gtest.h>

namespace {

// Minimal ApplicationServer for instantiation in tests
arangodb::application_features::ApplicationServer g_server;

}  // namespace

TEST(LicenseFeatureTest, OnlySuperUserReturnsFalse) {
  arangodb::LicenseFeature feature(g_server);
  EXPECT_FALSE(feature.onlySuperUser());
}

TEST(LicenseFeatureTest, IsEnterpriseReturnsTrue) {
  arangodb::LicenseFeature feature(g_server);
  EXPECT_TRUE(feature.isEnterprise());
}

TEST(LicenseFeatureTest, NameReturnsLicense) {
  EXPECT_EQ(arangodb::LicenseFeature::name(), "License");
}

TEST(LicenseFeatureTest, CanBeInstantiated) {
  // Verifies LicenseFeature is not abstract
  arangodb::LicenseFeature feature(g_server);
  (void)feature;
  SUCCEED();
}
