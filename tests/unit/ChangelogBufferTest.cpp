#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "Enterprise/RocksDBEngine/ChangelogBuffer.h"

using arangodb::ChangelogBuffer;
using arangodb::ChangelogEntry;
using arangodb::ChangelogOpType;

namespace {
constexpr uint64_t kLargeBudget = 100ULL * 1024 * 1024;  // 100 MB
}

// --- Append_SingleEntry_Retrievable ---
TEST(ChangelogBufferTest, Append_SingleEntry_Retrievable) {
  ChangelogBuffer buf(kLargeBudget);

  ChangelogEntry entry{ChangelogOpType::kInsert, "key1", "rev1", "data1"};
  ASSERT_TRUE(buf.append(entry));
  EXPECT_EQ(1u, buf.size());

  std::vector<ChangelogEntry> collected;
  buf.forEach([&](ChangelogEntry const& e) { collected.push_back(e); });

  ASSERT_EQ(1u, collected.size());
  EXPECT_EQ(ChangelogOpType::kInsert, collected[0].opType);
  EXPECT_EQ("key1", collected[0].documentKey);
  EXPECT_EQ("rev1", collected[0].documentRev);
  EXPECT_EQ("data1", collected[0].documentData);
}

// --- Append_MultipleEntries_OrderPreserved ---
TEST(ChangelogBufferTest, Append_MultipleEntries_OrderPreserved) {
  ChangelogBuffer buf(kLargeBudget);

  for (int i = 0; i < 10; ++i) {
    ChangelogEntry entry{ChangelogOpType::kInsert,
                         "key" + std::to_string(i),
                         "rev" + std::to_string(i),
                         "data" + std::to_string(i)};
    ASSERT_TRUE(buf.append(std::move(entry)));
  }

  EXPECT_EQ(10u, buf.size());

  int idx = 0;
  buf.forEach([&](ChangelogEntry const& e) {
    EXPECT_EQ("key" + std::to_string(idx), e.documentKey);
    ++idx;
  });
  EXPECT_EQ(10, idx);
}

// --- Append_DeleteEntry_TrackedByRev ---
TEST(ChangelogBufferTest, Append_DeleteEntry_TrackedByRev) {
  ChangelogBuffer buf(kLargeBudget);

  ChangelogEntry entry{ChangelogOpType::kDelete, "docKey", "rev42", ""};
  ASSERT_TRUE(buf.append(entry));

  std::vector<ChangelogEntry> collected;
  buf.forEach([&](ChangelogEntry const& e) { collected.push_back(e); });

  ASSERT_EQ(1u, collected.size());
  EXPECT_EQ(ChangelogOpType::kDelete, collected[0].opType);
  EXPECT_EQ("docKey", collected[0].documentKey);
  EXPECT_EQ("rev42", collected[0].documentRev);
  EXPECT_TRUE(collected[0].documentData.empty());
}

// --- Append_ConcurrentWriters_NoDataLoss ---
TEST(ChangelogBufferTest, Append_ConcurrentWriters_NoDataLoss) {
  ChangelogBuffer buf(kLargeBudget);

  constexpr int kThreads = 4;
  constexpr int kEntriesPerThread = 1000;

  std::vector<std::thread> threads;
  threads.reserve(kThreads);

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&buf, t]() {
      for (int i = 0; i < kEntriesPerThread; ++i) {
        std::string suffix =
            std::to_string(t) + "_" + std::to_string(i);
        ChangelogEntry entry{ChangelogOpType::kInsert,
                             "key_" + suffix,
                             "rev_" + suffix,
                             "data_" + suffix};
        EXPECT_TRUE(buf.append(std::move(entry)));
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(static_cast<uint64_t>(kThreads * kEntriesPerThread), buf.size());
}

// --- MemoryBudget_ExceedsLimit_ReturnsError ---
TEST(ChangelogBufferTest, MemoryBudget_ExceedsLimit_ReturnsError) {
  // Tiny budget: enough for ~1 entry, not 100
  ChangelogBuffer buf(200);

  bool firstAppendOk = false;
  bool anyRejected = false;

  for (int i = 0; i < 100; ++i) {
    ChangelogEntry entry{ChangelogOpType::kInsert,
                         "key" + std::to_string(i),
                         "rev" + std::to_string(i),
                         std::string(50, 'x')};
    bool ok = buf.append(std::move(entry));
    if (i == 0) {
      firstAppendOk = ok;
    }
    if (!ok) {
      anyRejected = true;
    }
  }

  // The budget is too small to hold all entries
  EXPECT_TRUE(anyRejected);
  // Memory usage should not exceed budget significantly
  EXPECT_LE(buf.memoryUsage(), 200u + 200u);  // some slack for overhead
}

// --- Clear_AfterAppend_IsEmpty ---
TEST(ChangelogBufferTest, Clear_AfterAppend_IsEmpty) {
  ChangelogBuffer buf(kLargeBudget);

  for (int i = 0; i < 5; ++i) {
    ChangelogEntry entry{ChangelogOpType::kInsert,
                         "key" + std::to_string(i), "rev", "data"};
    ASSERT_TRUE(buf.append(std::move(entry)));
  }

  EXPECT_EQ(5u, buf.size());
  EXPECT_GT(buf.memoryUsage(), 0u);

  buf.clear();

  EXPECT_EQ(0u, buf.size());
  EXPECT_EQ(0u, buf.memoryUsage());

  // Verify forEach yields nothing
  int count = 0;
  buf.forEach([&](ChangelogEntry const&) { ++count; });
  EXPECT_EQ(0, count);
}
