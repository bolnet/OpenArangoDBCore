#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <unordered_set>

#include "Enterprise/VocBase/SmartGraphSchema.h"

using namespace arangodb;

// --- extractSmartValue tests ---

TEST(SmartGraphSchema, ExtractSmartValue_WithColon) {
  auto val = SmartGraphSchema::extractSmartValue("eu:12345");
  EXPECT_EQ(val, "eu");
}

TEST(SmartGraphSchema, ExtractSmartValue_WithoutColon) {
  auto val = SmartGraphSchema::extractSmartValue("12345");
  EXPECT_TRUE(val.empty());
}

// --- buildSmartKey tests ---

TEST(SmartGraphSchema, BuildSmartKey) {
  auto key = SmartGraphSchema::buildSmartKey("eu", "12345");
  EXPECT_EQ(key, "eu:12345");
}

// --- buildSmartEdgeKey tests ---

TEST(SmartGraphSchema, BuildSmartEdgeKey) {
  auto key = SmartGraphSchema::buildSmartEdgeKey("eu", "us", "999");
  EXPECT_EQ(key, "eu:us:999");
}

// --- validateDocument tests ---

TEST(SmartGraphSchema, ValidateDocument_ValidKey_Accepts) {
  auto result =
      SmartGraphSchema::validateDocument("eu:12345", "eu", "smartAttr");
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(static_cast<bool>(result));
}

TEST(SmartGraphSchema, ValidateDocument_MissingColon_Rejects) {
  auto result =
      SmartGraphSchema::validateDocument("12345", "eu", "smartAttr");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(SmartGraphSchema, ValidateDocument_PrefixMismatch_Rejects) {
  auto result =
      SmartGraphSchema::validateDocument("us:12345", "eu", "smartAttr");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(SmartGraphSchema, ValidateDocument_EmptyAttribute_Rejects) {
  auto result =
      SmartGraphSchema::validateDocument("eu:12345", "", "smartAttr");
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

// --- validateEdge tests ---

TEST(SmartGraphSchema, ValidateEdge_Disjoint_SamePartition_Accepts) {
  auto result =
      SmartGraphSchema::validateEdge("eu:a", "eu:b", /*isDisjoint=*/true);
  EXPECT_TRUE(result.ok);
}

TEST(SmartGraphSchema, ValidateEdge_Disjoint_CrossPartition_Rejects) {
  auto result =
      SmartGraphSchema::validateEdge("eu:a", "us:b", /*isDisjoint=*/true);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(SmartGraphSchema, ValidateEdge_NonDisjoint_CrossPartition_Accepts) {
  auto result =
      SmartGraphSchema::validateEdge("eu:a", "us:b", /*isDisjoint=*/false);
  EXPECT_TRUE(result.ok);
}

TEST(SmartGraphSchema, ValidateEdge_SmartToSat_Accepts) {
  std::unordered_set<std::string> satellites{"satColl"};
  auto result = SmartGraphSchema::validateEdge(
      "eu:a", "satColl/xyz", /*isDisjoint=*/true, satellites);
  EXPECT_TRUE(result.ok);
}

// --- canModifySmartGraphAttribute ---

TEST(SmartGraphSchema, CanModifySmartGraphAttribute_ReturnsFalse) {
  EXPECT_FALSE(SmartGraphSchema::canModifySmartGraphAttribute());
}
