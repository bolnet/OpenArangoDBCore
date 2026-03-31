#include "Enterprise/Maskings/AttributeMasking.h"
#include "Enterprise/Maskings/AttributeMaskingEE.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using namespace arangodb::maskings;

class MaskingStrategiesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AttributeMasking::clearMaskings();
    InstallMaskingsEE();
  }

  void TearDown() override {
    AttributeMasking::clearMaskings();
  }
};

// ============================================================
// InstallMaskingsEE registration tests
// ============================================================

TEST_F(MaskingStrategiesTest, InstallMaskingsEERegisters4Strategies) {
  auto names = AttributeMasking::registeredNames();
  ASSERT_EQ(names.size(), 4u);
  EXPECT_NE(std::find(names.begin(), names.end(), "xifyFront"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "email"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "creditCard"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "phone"), names.end());
}

TEST_F(MaskingStrategiesTest, FindMaskingReturnsNullptrForUnknown) {
  EXPECT_EQ(AttributeMasking::findMasking("nonexistent"), nullptr);
}

TEST_F(MaskingStrategiesTest, FindMaskingReturnsFactoryForRegistered) {
  EXPECT_NE(AttributeMasking::findMasking("xifyFront"), nullptr);
  EXPECT_NE(AttributeMasking::findMasking("email"), nullptr);
  EXPECT_NE(AttributeMasking::findMasking("creditCard"), nullptr);
  EXPECT_NE(AttributeMasking::findMasking("phone"), nullptr);
}

// ============================================================
// XifyFront strategy tests
// ============================================================

TEST_F(MaskingStrategiesTest, XifyFrontMasksCharacters) {
  XifyFrontMask mask;
  EXPECT_EQ(mask.mask("John Doe"), "xxxx xxx");
}

TEST_F(MaskingStrategiesTest, XifyFrontPreservesLength) {
  XifyFrontMask mask;
  std::string input = "Hello World";
  EXPECT_EQ(mask.mask(input).size(), input.size());
}

TEST_F(MaskingStrategiesTest, XifyFrontPreservesSpaces) {
  XifyFrontMask mask;
  EXPECT_EQ(mask.mask("A B C"), "x x x");
}

TEST_F(MaskingStrategiesTest, XifyFrontEmptyString) {
  XifyFrontMask mask;
  EXPECT_EQ(mask.mask(""), "");
}

TEST_F(MaskingStrategiesTest, XifyFrontAllSpaces) {
  XifyFrontMask mask;
  EXPECT_EQ(mask.mask("   "), "   ");
}

// ============================================================
// Email strategy tests
// ============================================================

TEST_F(MaskingStrategiesTest, EmailProducesInvalidDomain) {
  EmailMask mask;
  std::string result = mask.mask("user@example.com");
  EXPECT_NE(result.find("@"), std::string::npos);
  EXPECT_NE(result.find(".invalid"), std::string::npos);
}

TEST_F(MaskingStrategiesTest, EmailProducesDotSeparatedFormat) {
  EmailMask mask;
  std::string result = mask.mask("user@example.com");
  // Format: AAAA.BBBB@CCCC.invalid
  auto atPos = result.find('@');
  ASSERT_NE(atPos, std::string::npos);
  auto localPart = result.substr(0, atPos);
  EXPECT_NE(localPart.find('.'), std::string::npos);
}

TEST_F(MaskingStrategiesTest, EmailIsDeterministic) {
  EmailMask mask;
  std::string result1 = mask.mask("user@example.com");
  std::string result2 = mask.mask("user@example.com");
  EXPECT_EQ(result1, result2);
}

TEST_F(MaskingStrategiesTest, EmailDifferentInputsDifferentOutput) {
  EmailMask mask;
  std::string result1 = mask.mask("alice@example.com");
  std::string result2 = mask.mask("bob@example.com");
  EXPECT_NE(result1, result2);
}

// ============================================================
// CreditCard strategy tests
// ============================================================

TEST_F(MaskingStrategiesTest, CreditCardMasksAllButLast4) {
  CreditCardMask mask;
  EXPECT_EQ(mask.mask("4111111111111111"), "xxxxxxxxxxxx1111");
}

TEST_F(MaskingStrategiesTest, CreditCardPreservesLength) {
  CreditCardMask mask;
  std::string input = "4111111111111111";
  EXPECT_EQ(mask.mask(input).size(), input.size());
}

TEST_F(MaskingStrategiesTest, CreditCardShortNumber) {
  CreditCardMask mask;
  // 4 digits or fewer: nothing to mask
  EXPECT_EQ(mask.mask("1234"), "1234");
}

TEST_F(MaskingStrategiesTest, CreditCardWith5Digits) {
  CreditCardMask mask;
  EXPECT_EQ(mask.mask("12345"), "x2345");
}

// ============================================================
// Phone strategy tests
// ============================================================

TEST_F(MaskingStrategiesTest, PhoneMasksAllButLast4Digits) {
  PhoneMask mask;
  // "+1-555-123-4567": '+' is non-separator->masked to 'x', 11 digits total,
  // last 4 preserved. Result: "xx-xxx-xxx-4567"
  EXPECT_EQ(mask.mask("+1-555-123-4567"), "xx-xxx-xxx-4567");
}

TEST_F(MaskingStrategiesTest, PhonePreservesSeparators) {
  PhoneMask mask;
  std::string result = mask.mask("+1-555-123-4567");
  // Count dashes
  int dashes = 0;
  for (char c : result) {
    if (c == '-') ++dashes;
  }
  EXPECT_EQ(dashes, 3);
}

TEST_F(MaskingStrategiesTest, PhoneWithSpaceSeparators) {
  PhoneMask mask;
  std::string result = mask.mask("555 123 4567");
  // Last 4 digits preserved, spaces preserved
  EXPECT_TRUE(result.size() == 12);
  EXPECT_EQ(result.substr(result.size() - 4), "4567");
}

TEST_F(MaskingStrategiesTest, PhoneShortNumber) {
  PhoneMask mask;
  EXPECT_EQ(mask.mask("4567"), "4567");
}

// ============================================================
// Config loading tests
// ============================================================

TEST_F(MaskingStrategiesTest, LoadConfigParsesCollections) {
  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "email", "type": "email" },
          { "path": "ssn", "type": "xifyFront" }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  ASSERT_EQ(configs.size(), 1u);
  EXPECT_EQ(configs[0].collectionName, "users");
  ASSERT_EQ(configs[0].rules.size(), 2u);
  EXPECT_EQ(configs[0].rules[0].path, "email");
  EXPECT_EQ(configs[0].rules[0].type, "email");
  EXPECT_EQ(configs[0].rules[1].path, "ssn");
  EXPECT_EQ(configs[0].rules[1].type, "xifyFront");
}

TEST_F(MaskingStrategiesTest, LoadConfigParsesRoles) {
  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "ssn", "type": "xifyFront", "roles": ["viewer", "analyst"] }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  ASSERT_EQ(configs.size(), 1u);
  ASSERT_EQ(configs[0].rules.size(), 1u);
  ASSERT_EQ(configs[0].rules[0].roles.size(), 2u);
  EXPECT_EQ(configs[0].rules[0].roles[0], "viewer");
  EXPECT_EQ(configs[0].rules[0].roles[1], "analyst");
}

TEST_F(MaskingStrategiesTest, LoadConfigEmptyCollections) {
  std::string json = R"({ "collections": {} })";
  auto configs = AttributeMasking::loadConfigFromJson(json);
  EXPECT_EQ(configs.size(), 0u);
}

TEST_F(MaskingStrategiesTest, LoadConfigNoCollectionsKey) {
  std::string json = R"({ "other": "data" })";
  auto configs = AttributeMasking::loadConfigFromJson(json);
  EXPECT_EQ(configs.size(), 0u);
}

// ============================================================
// Per-role masking tests
// ============================================================

TEST_F(MaskingStrategiesTest, RuleAppliesToAllRolesWhenEmpty) {
  MaskingRule rule{"email", "email", {}};
  EXPECT_TRUE(rule.appliesToRole("admin"));
  EXPECT_TRUE(rule.appliesToRole("viewer"));
  EXPECT_TRUE(rule.appliesToRole(""));
}

TEST_F(MaskingStrategiesTest, RuleAppliesToSpecificRoles) {
  MaskingRule rule{"ssn", "xifyFront", {"viewer", "analyst"}};
  EXPECT_TRUE(rule.appliesToRole("viewer"));
  EXPECT_TRUE(rule.appliesToRole("analyst"));
  EXPECT_FALSE(rule.appliesToRole("admin"));
  EXPECT_FALSE(rule.appliesToRole(""));
}

// ============================================================
// Document-level masking (applyMasking) tests
// ============================================================

TEST_F(MaskingStrategiesTest, ApplyMaskingRedactsConfiguredFields) {
  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "email", "type": "email" },
          { "path": "name", "type": "xifyFront" }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  std::unordered_map<std::string, std::string> fields = {
      {"email", "alice@example.com"},
      {"name", "Alice"},
      {"id", "12345"}
  };

  auto result = AttributeMasking::applyMasking("users", "viewer", fields,
                                                 configs);

  // email should be masked
  EXPECT_NE(result["email"], "alice@example.com");
  EXPECT_NE(result["email"].find(".invalid"), std::string::npos);

  // name should be masked
  EXPECT_EQ(result["name"], "xxxxx");

  // id should be unchanged (no rule for it)
  EXPECT_EQ(result["id"], "12345");
}

TEST_F(MaskingStrategiesTest, ApplyMaskingRespectsRoles) {
  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "ssn", "type": "xifyFront", "roles": ["viewer"] }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  std::unordered_map<std::string, std::string> fields = {
      {"ssn", "123-45-6789"}
  };

  // Viewer role: should be masked
  auto viewerResult = AttributeMasking::applyMasking("users", "viewer",
                                                      fields, configs);
  EXPECT_NE(viewerResult["ssn"], "123-45-6789");

  // Admin role: should NOT be masked (rule restricted to viewer)
  auto adminResult = AttributeMasking::applyMasking("users", "admin",
                                                     fields, configs);
  EXPECT_EQ(adminResult["ssn"], "123-45-6789");
}

TEST_F(MaskingStrategiesTest, ApplyMaskingIgnoresOtherCollections) {
  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "email", "type": "email" }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  std::unordered_map<std::string, std::string> fields = {
      {"email", "alice@example.com"}
  };

  // Different collection: should not be masked
  auto result = AttributeMasking::applyMasking("orders", "viewer",
                                                fields, configs);
  EXPECT_EQ(result["email"], "alice@example.com");
}

TEST_F(MaskingStrategiesTest, ApplyMaskingUnknownStrategyIgnored) {
  AttributeMasking::clearMaskings();
  // Don't register strategies

  std::string json = R"({
    "collections": {
      "users": {
        "rules": [
          { "path": "email", "type": "email" }
        ]
      }
    }
  })";

  auto configs = AttributeMasking::loadConfigFromJson(json);
  std::unordered_map<std::string, std::string> fields = {
      {"email", "alice@example.com"}
  };

  auto result = AttributeMasking::applyMasking("users", "viewer",
                                                fields, configs);
  // Unknown strategy: field unchanged
  EXPECT_EQ(result["email"], "alice@example.com");
}
