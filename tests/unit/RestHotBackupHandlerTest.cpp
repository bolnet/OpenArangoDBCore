#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "Enterprise/RestHandler/RestHotBackupHandler.h"
#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"
#include "HotBackupMocks.h"

namespace fs = std::filesystem;

namespace {

std::string makeTempDir(std::string const& prefix) {
  auto path = fs::temp_directory_path() /
              (prefix + "_" +
               std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  return path.string();
}

void cleanupDir(std::string const& path) {
  std::error_code ec;
  fs::remove_all(path, ec);
}

void createFakeSstFile(std::string const& dir) {
  std::ofstream ofs(dir + "/000001.sst");
  ofs << "fake-sst-data";
  ofs.close();
}

}  // namespace

// ============================================================================
// ParseOperation Tests
// ============================================================================

TEST(RestHotBackupHandler, ParseOperation_Create_ReturnsCreate) {
  auto op = arangodb::RestHotBackupHandler::parseOperation("create");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::CREATE, op);
}

TEST(RestHotBackupHandler, ParseOperation_List_ReturnsList) {
  auto op = arangodb::RestHotBackupHandler::parseOperation("list");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::LIST, op);
}

TEST(RestHotBackupHandler, ParseOperation_Delete_ReturnsDelete) {
  auto op = arangodb::RestHotBackupHandler::parseOperation("delete");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::DELETE, op);
}

TEST(RestHotBackupHandler, ParseOperation_Restore_ReturnsRestore) {
  auto op = arangodb::RestHotBackupHandler::parseOperation("restore");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::RESTORE, op);
}

TEST(RestHotBackupHandler, ParseOperation_Unknown_ReturnsUnknown) {
  auto op = arangodb::RestHotBackupHandler::parseOperation("foo");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::UNKNOWN, op);

  op = arangodb::RestHotBackupHandler::parseOperation("");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::UNKNOWN, op);

  op = arangodb::RestHotBackupHandler::parseOperation("CREATE");
  EXPECT_EQ(arangodb::RestHotBackupHandler::Operation::UNKNOWN, op);
}

// ============================================================================
// ExecuteCreate Tests
// ============================================================================

class RestHandlerCreateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("rest_create_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
    _handler = std::make_unique<arangodb::RestHotBackupHandler>(*_backup);
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
  std::unique_ptr<arangodb::RestHotBackupHandler> _handler;
};

TEST_F(RestHandlerCreateTest, Success_ReturnsManifest) {
  std::string response;
  auto res = _handler->executeCreate("test-label", response);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  // Response should contain success envelope
  EXPECT_NE(std::string::npos, response.find("\"error\": false"));
  EXPECT_NE(std::string::npos, response.find("\"code\": 200"));
  EXPECT_NE(std::string::npos, response.find("\"id\":"));
  EXPECT_NE(std::string::npos, response.find("\"datetime\":"));
}

TEST_F(RestHandlerCreateTest, Failure_ReturnsError) {
  _mockDb->checkpointShouldSucceed = false;

  std::string response;
  auto res = _handler->executeCreate("test-label", response);
  EXPECT_FALSE(res.ok());

  EXPECT_NE(std::string::npos, response.find("\"error\": true"));
}

// ============================================================================
// ExecuteList Tests
// ============================================================================

class RestHandlerListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("rest_list_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
    _handler = std::make_unique<arangodb::RestHotBackupHandler>(*_backup);
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
  std::unique_ptr<arangodb::RestHotBackupHandler> _handler;
};

TEST_F(RestHandlerListTest, Empty_ReturnsEmptyArray) {
  std::string response;
  auto res = _handler->executeList(response);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_NE(std::string::npos, response.find("\"error\": false"));
  EXPECT_NE(std::string::npos, response.find("[]"));
}

TEST_F(RestHandlerListTest, MultipleBackups_ReturnsAll) {
  // Create two backups
  for (int i = 0; i < 2; ++i) {
    arangodb::RocksDBHotBackup::CreateResult result;
    auto createRes = _backup->create("bk-" + std::to_string(i), result);
    ASSERT_TRUE(createRes.ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::string response;
  auto res = _handler->executeList(response);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  // Should contain two backup entries
  // Count occurrences of "id"
  size_t count = 0;
  size_t pos = 0;
  while ((pos = response.find("\"id\":", pos)) != std::string::npos) {
    ++count;
    ++pos;
  }
  EXPECT_EQ(2u, count) << "Should have 2 backup entries in response";
}

// ============================================================================
// ExecuteDelete Tests
// ============================================================================

class RestHandlerDeleteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("rest_delete_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
    _handler = std::make_unique<arangodb::RestHotBackupHandler>(*_backup);
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
  std::unique_ptr<arangodb::RestHotBackupHandler> _handler;
};

TEST_F(RestHandlerDeleteTest, Success_ReturnsOk) {
  // Create a backup to delete
  arangodb::RocksDBHotBackup::CreateResult result;
  auto createRes = _backup->create("to-delete", result);
  ASSERT_TRUE(createRes.ok());

  std::string response;
  auto deleteRes =
      _handler->executeDelete(result.manifest.backupId, response);
  ASSERT_TRUE(deleteRes.ok()) << deleteRes.errorMessage();

  EXPECT_NE(std::string::npos, response.find("\"deleted\": true"));
}

TEST_F(RestHandlerDeleteTest, NotFound_Returns404) {
  std::string response;
  auto res = _handler->executeDelete("nonexistent", response);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(404, res.errorNumber());

  EXPECT_NE(std::string::npos, response.find("\"error\": true"));
  EXPECT_NE(std::string::npos, response.find("404"));
}

// ============================================================================
// ExecuteRestore Tests
// ============================================================================

class RestHandlerRestoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("rest_restore_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
    _handler = std::make_unique<arangodb::RestHotBackupHandler>(*_backup);
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
  std::unique_ptr<arangodb::RestHotBackupHandler> _handler;
};

TEST_F(RestHandlerRestoreTest, Success_ReturnsOk) {
  // Create a valid backup
  arangodb::RocksDBHotBackup::CreateResult result;
  auto createRes = _backup->create("for-restore", result);
  ASSERT_TRUE(createRes.ok());

  // Add fake SST file
  createFakeSstFile(result.manifest.path);

  std::string response;
  auto restoreRes =
      _handler->executeRestore(result.manifest.backupId, response);
  ASSERT_TRUE(restoreRes.ok()) << restoreRes.errorMessage();

  EXPECT_NE(std::string::npos, response.find("\"restored\": true"));
}
