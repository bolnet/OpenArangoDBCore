#include "Enterprise/Replication/ReplicationLagMonitor.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace arangodb;

namespace {

/// A fixed clock that returns a configurable timestamp.
class FixedClock {
 public:
  explicit FixedClock(uint64_t initialMicros) : _now(initialMicros) {}

  ClockProvider provider() {
    return [this]() -> uint64_t { return _now; };
  }

  void setNow(uint64_t micros) { _now = micros; }
  void advance(uint64_t deltaMicros) { _now += deltaMicros; }

 private:
  uint64_t _now;
};

}  // namespace

// Test 17: Before any entries are recorded, lag reports zero
TEST(ReplicationLagMonitorTest, NoEntries_LagIsZero) {
  FixedClock clock(10'000'000);  // 10 seconds in micros
  ReplicationLagMonitor monitor(30.0, clock.provider());

  auto lag = monitor.getLag("shard_0");
  EXPECT_EQ(lag.shardId, "shard_0");
  EXPECT_DOUBLE_EQ(lag.lagSeconds, 0.0);
  EXPECT_FALSE(lag.exceedsThreshold);
  EXPECT_FALSE(monitor.anyShardExceedsThreshold());
}

// Test 18: Recording an entry with a past timestamp yields positive lag
TEST(ReplicationLagMonitorTest, RecordEntry_ComputesLag) {
  FixedClock clock(20'000'000);  // 20 seconds in micros
  ReplicationLagMonitor monitor(30.0, clock.provider());

  // Entry was written at 15 seconds; current time is 20 seconds
  monitor.recordEntry("shard_0", 15'000'000);

  auto lag = monitor.getLag("shard_0");
  EXPECT_NEAR(lag.lagSeconds, 5.0, 0.001);
  EXPECT_FALSE(lag.exceedsThreshold);

  // Advance clock by 10 more seconds (lag = 15s)
  clock.advance(10'000'000);
  lag = monitor.getLag("shard_0");
  EXPECT_NEAR(lag.lagSeconds, 15.0, 0.001);
  EXPECT_FALSE(lag.exceedsThreshold);
}

// Test 19: Each shard has its own lag value
TEST(ReplicationLagMonitorTest, MultipleShards_IndependentLag) {
  FixedClock clock(100'000'000);  // 100 seconds
  ReplicationLagMonitor monitor(30.0, clock.provider());

  // Shard 0: entry at 90s -> lag = 10s
  monitor.recordEntry("shard_0", 90'000'000);
  // Shard 1: entry at 50s -> lag = 50s
  monitor.recordEntry("shard_1", 50'000'000);

  auto lag0 = monitor.getLag("shard_0");
  auto lag1 = monitor.getLag("shard_1");

  EXPECT_NEAR(lag0.lagSeconds, 10.0, 0.001);
  EXPECT_NEAR(lag1.lagSeconds, 50.0, 0.001);

  // Only shard_1 should exceed threshold (50s > 30s)
  EXPECT_FALSE(lag0.exceedsThreshold);
  EXPECT_TRUE(lag1.exceedsThreshold);

  auto allLags = monitor.getAllLags();
  EXPECT_EQ(allLags.size(), 2u);
}

// Test 20: Lag exceeding lagToleranceSeconds triggers alert flag
TEST(ReplicationLagMonitorTest, ExceedsThreshold_ReportsAlert) {
  FixedClock clock(60'000'000);  // 60 seconds
  ReplicationLagMonitor monitor(10.0, clock.provider());  // threshold = 10s

  // Entry at 55s -> lag = 5s (under threshold)
  monitor.recordEntry("shard_0", 55'000'000);
  EXPECT_FALSE(monitor.anyShardExceedsThreshold());

  auto lag = monitor.getLag("shard_0");
  EXPECT_NEAR(lag.lagSeconds, 5.0, 0.001);
  EXPECT_FALSE(lag.exceedsThreshold);

  // Advance clock to 80s -> lag = 25s (over threshold of 10s)
  clock.setNow(80'000'000);

  lag = monitor.getLag("shard_0");
  EXPECT_NEAR(lag.lagSeconds, 25.0, 0.001);
  EXPECT_TRUE(lag.exceedsThreshold);
  EXPECT_TRUE(monitor.anyShardExceedsThreshold());
}
