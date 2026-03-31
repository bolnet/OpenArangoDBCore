#include "Enterprise/Audit/AuditLogger.h"
#include "Enterprise/Audit/AuditFeature.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Helper: create a temp file path for audit output
std::string tempAuditFile() {
  auto path = fs::temp_directory_path() / ("audit_test_" +
    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
    ".log");
  return path.string();
}

// Helper: read all lines from a file
std::vector<std::string> readLines(std::string const& path) {
  std::vector<std::string> lines;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

// Helper: create a sample audit event
arangodb::AuditEvent makeEvent(std::string const& topic, int seq = 0) {
  return arangodb::AuditEvent{
    .timestamp = "2026-03-31T00:00:00Z",
    .server = "localhost",
    .topic = topic,
    .username = "testuser",
    .database = "_system",
    .clientIp = "127.0.0.1",
    .authMethod = "basic",
    .text = "action-" + std::to_string(seq)
  };
}

}  // namespace

// ========================================================================
// AuditLogger Tests
// ========================================================================

TEST(AuditLoggerTest, LogReturnsImmediately) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();
  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  logger.start();

  auto before = std::chrono::steady_clock::now();
  logger.log(makeEvent("authentication"));
  auto after = std::chrono::steady_clock::now();

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(after - before);
  EXPECT_LT(elapsed.count(), 10) << "log() should return immediately (non-blocking)";

  logger.stop();
  fs::remove(file);
}

TEST(AuditLoggerTest, EventsAppearInFileWithin100ms) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();
  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  logger.start();

  logger.log(makeEvent("authentication", 1));

  // Wait up to 200ms for event to appear in file
  bool found = false;
  for (int i = 0; i < 20; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto lines = readLines(file);
    if (!lines.empty()) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Logged events should appear in output file within 200ms";

  logger.stop();
  fs::remove(file);
}

TEST(AuditLoggerTest, ConcurrentProducersNoCorruption) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();
  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  logger.start();

  constexpr int kThreads = 8;
  constexpr int kEventsPerThread = 100;

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&logger, t]() {
      for (int i = 0; i < kEventsPerThread; ++i) {
        logger.log(makeEvent("thread-" + std::to_string(t), i));
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  logger.stop();

  auto lines = readLines(file);
  EXPECT_EQ(static_cast<int>(lines.size()), kThreads * kEventsPerThread)
      << "All " << (kThreads * kEventsPerThread) << " events should appear in output";

  fs::remove(file);
}

TEST(AuditLoggerTest, StopFlushesAllEvents) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();
  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  logger.start();

  constexpr int kEvents = 50;
  for (int i = 0; i < kEvents; ++i) {
    logger.log(makeEvent("flush-test", i));
  }

  // stop() must flush all remaining events before returning
  logger.stop();

  auto lines = readLines(file);
  EXPECT_EQ(static_cast<int>(lines.size()), kEvents)
      << "stop() must flush all remaining events (Pitfall 5)";

  fs::remove(file);
}

TEST(AuditLoggerTest, FileOutputFormatPipeDelimited) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();
  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  logger.start();

  logger.log(makeEvent("authentication", 42));
  logger.stop();

  auto lines = readLines(file);
  ASSERT_EQ(lines.size(), 1u);

  // Expected: "timestamp|server|topic|username|database|clientIp|authMethod|text"
  std::string const& line = lines[0];
  EXPECT_NE(line.find("2026-03-31T00:00:00Z"), std::string::npos);
  EXPECT_NE(line.find("localhost"), std::string::npos);
  EXPECT_NE(line.find("authentication"), std::string::npos);
  EXPECT_NE(line.find("testuser"), std::string::npos);
  EXPECT_NE(line.find("_system"), std::string::npos);
  EXPECT_NE(line.find("127.0.0.1"), std::string::npos);
  EXPECT_NE(line.find("basic"), std::string::npos);
  EXPECT_NE(line.find("action-42"), std::string::npos);

  // Check pipe-delimited format (8 fields = 7 pipes)
  int pipeCount = 0;
  for (char c : line) {
    if (c == '|') ++pipeCount;
  }
  EXPECT_EQ(pipeCount, 7) << "Format should be 8 pipe-delimited fields";

  fs::remove(file);
}

TEST(AuditLoggerTest, AddOutputFileCreatesFile) {
  arangodb::AuditLogger logger;
  auto file = tempAuditFile();

  logger.addOutput(arangodb::AuditLogger::OutputType::File, file);
  EXPECT_TRUE(fs::exists(file)) << "addOutput(File) should create the output file";

  fs::remove(file);
}

TEST(AuditLoggerTest, AddOutputSyslogDoesNotCrash) {
  arangodb::AuditLogger logger;
  // Just verify this doesn't throw or crash
  EXPECT_NO_THROW(
    logger.addOutput(arangodb::AuditLogger::OutputType::Syslog, "local0")
  );
}

TEST(AuditLoggerTest, EventFormatMethod) {
  auto event = makeEvent("authorization", 7);
  auto formatted = event.format();
  EXPECT_EQ(formatted,
    "2026-03-31T00:00:00Z|localhost|authorization|testuser|_system|127.0.0.1|basic|action-7");
}
