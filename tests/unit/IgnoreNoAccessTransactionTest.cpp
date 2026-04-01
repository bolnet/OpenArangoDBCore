#include <gtest/gtest.h>

#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"

#include <vector>

namespace arangodb {
namespace test {

// ==========================================================================
// IgnoreNoAccessAqlTransaction Tests
// ==========================================================================

TEST(IgnoreNoAccessAqlTransaction, CheckAccess_AlwaysGrantsWrite) {
  IgnoreNoAccessAqlTransaction trx;

  // AccessMode values: 0=NONE, 1=READ, 2=WRITE, 3=EXCLUSIVE
  EXPECT_EQ(0, trx.checkAccess("myCollection", 0));
  EXPECT_EQ(0, trx.checkAccess("myCollection", 1));
  EXPECT_EQ(0, trx.checkAccess("myCollection", 2));
  EXPECT_EQ(0, trx.checkAccess("myCollection", 3));
}

TEST(IgnoreNoAccessAqlTransaction, CheckAccess_ReadOnlyCollectionBypass) {
  IgnoreNoAccessAqlTransaction trx;

  // Even a read-only system collection should pass.
  EXPECT_EQ(0, trx.checkAccess("_system_readonly", 2));
  EXPECT_EQ(0, trx.checkAccess("_graphs", 3));
  EXPECT_EQ(0, trx.checkAccess("restricted_collection", 2));
}

TEST(IgnoreNoAccessAqlTransaction, Insert_BypassesPermission) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_EQ(0, trx.begin());
  // The transaction is active and allows all operations.
  EXPECT_TRUE(trx.isActive());
  EXPECT_EQ(0, trx.checkAccess("restricted_coll", 2));
  EXPECT_EQ(0, trx.commit());
}

TEST(IgnoreNoAccessAqlTransaction, Update_BypassesPermission) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_EQ(0, trx.begin());
  EXPECT_EQ(0, trx.checkAccess("restricted_coll", 2));
  EXPECT_EQ(0, trx.commit());
}

TEST(IgnoreNoAccessAqlTransaction, Remove_BypassesPermission) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_EQ(0, trx.begin());
  EXPECT_EQ(0, trx.checkAccess("restricted_coll", 2));
  EXPECT_EQ(0, trx.commit());
}

TEST(IgnoreNoAccessAqlTransaction, ScopedToInstance) {
  IgnoreNoAccessAqlTransaction replicationTrx;

  // The replication transaction always bypasses.
  EXPECT_TRUE(replicationTrx.isReplicationTransaction());
  EXPECT_EQ(0, replicationTrx.checkAccess("any_collection", 3));

  // A separate instance also bypasses (but that's the point --
  // normal transactions are a different class entirely).
  IgnoreNoAccessAqlTransaction anotherReplicationTrx;
  EXPECT_TRUE(anotherReplicationTrx.isReplicationTransaction());
  EXPECT_EQ(0, anotherReplicationTrx.checkAccess("any_collection", 3));
}

TEST(IgnoreNoAccessAqlTransaction, BeginCommitLifecycle) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_FALSE(trx.isActive());

  EXPECT_EQ(0, trx.begin());
  EXPECT_TRUE(trx.isActive());

  // Double begin fails.
  EXPECT_EQ(1, trx.begin());

  EXPECT_EQ(0, trx.commit());
  EXPECT_FALSE(trx.isActive());

  // Commit when not active fails.
  EXPECT_EQ(1, trx.commit());
}

TEST(IgnoreNoAccessAqlTransaction, AbortLifecycle) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_EQ(0, trx.begin());
  EXPECT_TRUE(trx.isActive());

  EXPECT_EQ(0, trx.abort());
  EXPECT_FALSE(trx.isActive());

  // Abort when not active is a safe no-op.
  EXPECT_EQ(0, trx.abort());
}

TEST(IgnoreNoAccessAqlTransaction, MoveSemantics) {
  IgnoreNoAccessAqlTransaction trx;
  EXPECT_EQ(0, trx.begin());
  EXPECT_TRUE(trx.isActive());

  IgnoreNoAccessAqlTransaction moved(std::move(trx));
  EXPECT_TRUE(moved.isActive());
  EXPECT_FALSE(trx.isActive());  // NOLINT: testing moved-from state

  IgnoreNoAccessAqlTransaction assigned;
  assigned = std::move(moved);
  EXPECT_TRUE(assigned.isActive());
}

// ==========================================================================
// IgnoreNoAccessMethods Tests
// ==========================================================================

TEST(IgnoreNoAccessMethods, Insert_WritesToReadOnlyCollection) {
  IgnoreNoAccessMethods methods;
  std::vector<uint8_t> doc = {1, 2, 3, 4};
  EXPECT_EQ(0, methods.insert("readonly_collection", doc));
  EXPECT_EQ(1u, methods.operationCount());
}

TEST(IgnoreNoAccessMethods, Update_WritesToReadOnlyCollection) {
  IgnoreNoAccessMethods methods;
  std::vector<uint8_t> doc = {5, 6, 7, 8};
  EXPECT_EQ(0, methods.update("readonly_collection", doc));
  EXPECT_EQ(1u, methods.operationCount());
}

TEST(IgnoreNoAccessMethods, Remove_WritesToReadOnlyCollection) {
  IgnoreNoAccessMethods methods;
  EXPECT_EQ(0, methods.remove("readonly_collection", "doc123"));
  EXPECT_EQ(1u, methods.operationCount());
}

TEST(IgnoreNoAccessMethods, Truncate_WritesToReadOnlyCollection) {
  IgnoreNoAccessMethods methods;
  EXPECT_EQ(0, methods.truncate("readonly_collection"));
  EXPECT_EQ(1u, methods.operationCount());
}

TEST(IgnoreNoAccessMethods, OnlyReplicationContext) {
  IgnoreNoAccessMethods methods;
  EXPECT_TRUE(methods.isReplicationContext());
}

TEST(IgnoreNoAccessMethods, EmptyCollectionName_ReturnsError) {
  IgnoreNoAccessMethods methods;
  std::vector<uint8_t> doc = {1};
  EXPECT_EQ(1, methods.insert("", doc));
  EXPECT_EQ(0u, methods.operationCount());
}

TEST(IgnoreNoAccessMethods, MoveSemantics) {
  IgnoreNoAccessMethods methods;
  std::vector<uint8_t> doc = {1};
  methods.insert("coll", doc);
  methods.insert("coll", doc);
  EXPECT_EQ(2u, methods.operationCount());

  IgnoreNoAccessMethods moved(std::move(methods));
  EXPECT_EQ(2u, moved.operationCount());
  EXPECT_EQ(0u, methods.operationCount());  // NOLINT: testing moved-from
}

}  // namespace test
}  // namespace arangodb
