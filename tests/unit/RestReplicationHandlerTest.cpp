#include <gtest/gtest.h>

#include "ReplicationMocks.h"
#include "Enterprise/RestHandler/RestReplicationHandler.h"

#include <string>

namespace arangodb {
namespace test {

using Handler =
    RestReplicationHandlerT<MockSequenceNumberTracker, MockShardWALTailer>;

// ========================================================================
// Route Parsing Tests
// ========================================================================

TEST(RestReplicationHandler, ParseRoute_Status_ReturnsStatusEnum) {
  EXPECT_EQ(Handler::parseRoute("status"), Handler::Route::STATUS);
}

TEST(RestReplicationHandler, ParseRoute_Start_ReturnsStartEnum) {
  EXPECT_EQ(Handler::parseRoute("start"), Handler::Route::START);
}

TEST(RestReplicationHandler, ParseRoute_Stop_ReturnsStopEnum) {
  EXPECT_EQ(Handler::parseRoute("stop"), Handler::Route::STOP);
}

TEST(RestReplicationHandler, ParseRoute_Reset_ReturnsResetEnum) {
  EXPECT_EQ(Handler::parseRoute("reset"), Handler::Route::RESET);
}

TEST(RestReplicationHandler, ParseRoute_Feed_ReturnsFeedEnum) {
  EXPECT_EQ(Handler::parseRoute("feed"), Handler::Route::FEED);
}

TEST(RestReplicationHandler, ParseRoute_Checkpoint_ReturnsCheckpointEnum) {
  EXPECT_EQ(Handler::parseRoute("checkpoint"), Handler::Route::CHECKPOINT);
}

TEST(RestReplicationHandler, ParseRoute_Unknown_ReturnsUnknownEnum) {
  EXPECT_EQ(Handler::parseRoute("bogus"), Handler::Route::UNKNOWN);
}

TEST(RestReplicationHandler, ParseRoute_Empty_ReturnsUnknownEnum) {
  EXPECT_EQ(Handler::parseRoute(""), Handler::Route::UNKNOWN);
}

// ========================================================================
// Envelope Formatting Tests
// ========================================================================

TEST(RestReplicationHandler, FormatSuccess_WrapsInEnvelope) {
  auto result = Handler::formatSuccess("{\"test\": true}");
  EXPECT_NE(result.find("\"error\": false"), std::string::npos);
  EXPECT_NE(result.find("\"code\": 200"), std::string::npos);
  EXPECT_NE(result.find("\"result\": {\"test\": true}"), std::string::npos);
}

TEST(RestReplicationHandler, FormatError_WrapsInEnvelope) {
  auto result = Handler::formatError(400, "bad request");
  EXPECT_NE(result.find("\"error\": true"), std::string::npos);
  EXPECT_NE(result.find("\"code\": 400"), std::string::npos);
  EXPECT_NE(result.find("\"errorMessage\": \"bad request\""),
            std::string::npos);
}

TEST(RestReplicationHandler, FormatError_DifferentCodes) {
  auto r409 = Handler::formatError(409, "conflict");
  EXPECT_NE(r409.find("\"code\": 409"), std::string::npos);

  auto r503 = Handler::formatError(503, "unavailable");
  EXPECT_NE(r503.find("\"code\": 503"), std::string::npos);
}

// ========================================================================
// Start/Stop Lifecycle Tests
// ========================================================================

TEST(RestReplicationHandler, ExecuteStart_WhenStopped_ReturnsConfirmation) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeStart(response);

  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
  EXPECT_NE(response.find("\"started\": true"), std::string::npos);
  EXPECT_NE(response.find("\"previousState\": \"stopped\""),
            std::string::npos);
  EXPECT_TRUE(tailer.isRunning());
}

TEST(RestReplicationHandler, ExecuteStop_WhenRunning_ReturnsConfirmation) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  tailer.start();  // put into running state
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeStop(response);

  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
  EXPECT_NE(response.find("\"stopped\": true"), std::string::npos);
  EXPECT_NE(response.find("\"previousState\": \"running\""),
            std::string::npos);
  EXPECT_FALSE(tailer.isRunning());
}

TEST(RestReplicationHandler, ExecuteStart_WhenAlreadyRunning_Returns409) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  tailer.start();  // already running
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeStart(response);

  EXPECT_NE(response.find("\"error\": true"), std::string::npos);
  EXPECT_NE(response.find("\"code\": 409"), std::string::npos);
  EXPECT_NE(response.find("replication is already running"),
            std::string::npos);
}

TEST(RestReplicationHandler, ExecuteStop_WhenAlreadyStopped_Returns409) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;  // not started
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeStop(response);

  EXPECT_NE(response.find("\"error\": true"), std::string::npos);
  EXPECT_NE(response.find("\"code\": 409"), std::string::npos);
  EXPECT_NE(response.find("replication is not running"), std::string::npos);
}

// ========================================================================
// Reset Tests
// ========================================================================

TEST(RestReplicationHandler, ExecuteReset_ClearsTrackingReturnsCount) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  tracker.addShard("shard-2", 200, 100);
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeReset(response);

  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
  EXPECT_NE(response.find("\"reset\": true"), std::string::npos);
  EXPECT_NE(response.find("\"shardsReset\": 2"), std::string::npos);

  // Verify tracker was actually reset
  EXPECT_EQ(tracker.getAllStates().size(), 0u);
}

// ========================================================================
// Feed Validation Tests
// ========================================================================

TEST(RestReplicationHandler, ExecuteFeed_EmptyShardId_Returns400) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeFeed("", 0, 100, response);

  EXPECT_NE(response.find("\"error\": true"), std::string::npos);
  EXPECT_NE(response.find("\"code\": 400"), std::string::npos);
  EXPECT_NE(response.find("missing required parameter: shardId"),
            std::string::npos);
}

TEST(RestReplicationHandler, ExecuteFeed_ZeroMaxCount_DefaultsTo1000) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeFeed("shard-1", 0, 0, response);

  // Should succeed (not error), since maxCount=0 defaults to 1000
  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
}

// ========================================================================
// Checkpoint Validation Tests
// ========================================================================

TEST(RestReplicationHandler, ExecuteCheckpoint_EmptyShardId_Returns400) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeCheckpoint("", 5, response);

  EXPECT_NE(response.find("\"error\": true"), std::string::npos);
  EXPECT_NE(response.find("\"code\": 400"), std::string::npos);
  EXPECT_NE(response.find("missing required field: shardId"),
            std::string::npos);
}

TEST(RestReplicationHandler, ExecuteCheckpoint_ZeroSequence_Returns400) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeCheckpoint("shard-1", 0, response);

  EXPECT_NE(response.find("\"error\": true"), std::string::npos);
  EXPECT_NE(response.find("\"code\": 400"), std::string::npos);
  EXPECT_NE(response.find("appliedSequence must be greater than 0"),
            std::string::npos);
}

TEST(RestReplicationHandler, ExecuteCheckpoint_ValidInput_ReturnsAck) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  MockShardWALTailer tailer;
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeCheckpoint("shard-1", 75, response);

  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
  EXPECT_NE(response.find("\"acknowledged\": true"), std::string::npos);
  EXPECT_NE(response.find("\"shardId\": \"shard-1\""), std::string::npos);
  EXPECT_NE(response.find("\"appliedSequence\": 75"), std::string::npos);
}

// ========================================================================
// Status Delegation Test
// ========================================================================

TEST(RestReplicationHandler, ExecuteStatus_DelegatesToStatusBuilder) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 80);
  MockShardWALTailer tailer;
  tailer.setSent(50);
  tailer.setAcked(40);
  Handler handler(tracker, tailer);

  std::string response;
  handler.executeStatus(response);

  EXPECT_NE(response.find("\"error\": false"), std::string::npos);
  EXPECT_NE(response.find("\"isRunning\":"), std::string::npos);
  EXPECT_NE(response.find("\"shardStates\":"), std::string::npos);
  EXPECT_NE(response.find("\"totalPending\":"), std::string::npos);
}

}  // namespace test
}  // namespace arangodb
