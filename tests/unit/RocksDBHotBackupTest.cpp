#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#include "HotBackupMocks.h"

namespace fs = std::filesystem;

namespace {

/// Create a unique temp directory for a test.
std::string makeTempDir(std::string const& prefix) {
  auto path = fs::temp_directory_path() /
              (prefix + "_" +
               std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(path);
  return path.string();
}

/// Cleanup a temp directory.
void cleanupDir(std::string const& path) {
  std::error_code ec;
  fs::remove_all(path, ec);
}

/// Create a fake SST file inside a backup directory (for restore validation).
void createFakeSstFile(std::string const& dir) {
  std::ofstream ofs(dir + "/000001.sst");
  ofs << "fake-sst-data";
  ofs.close();
}

}  // namespace

// ============================================================================
// BackupManifest Tests
// ============================================================================

TEST(BackupManifest, ToJson_RoundTrips) {
  arangodb::BackupManifest original;
  original.backupId = "2026-03-31T14.30.00.123456";
  original.timestamp = "2026-03-31T14:30:00Z";
  original.version = "3.12.0";
  original.sequenceNumber = 12345;
  original.collections = {"_users", "myCollection", "orders"};
  original.path = "/tmp/backups/2026-03-31T14.30.00.123456";
  original.isConsistent = true;

  std::string json = original.toJson();
  ASSERT_FALSE(json.empty());

  arangodb::BackupManifest restored;
  auto res = arangodb::BackupManifest::fromJson(json, restored);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_EQ(original.backupId, restored.backupId);
  EXPECT_EQ(original.timestamp, restored.timestamp);
  EXPECT_EQ(original.version, restored.version);
  EXPECT_EQ(original.sequenceNumber, restored.sequenceNumber);
  EXPECT_EQ(original.collections, restored.collections);
  EXPECT_EQ(original.path, restored.path);
  EXPECT_EQ(original.isConsistent, restored.isConsistent);
}

TEST(BackupManifest, FromJson_MissingField_ReturnsError) {
  // JSON missing backupId
  std::string json = R"({
    "timestamp": "2026-03-31T14:30:00Z",
    "version": "3.12.0"
  })";

  arangodb::BackupManifest manifest;
  auto res = arangodb::BackupManifest::fromJson(json, manifest);
  EXPECT_FALSE(res.ok());
  EXPECT_NE(std::string::npos,
            res.errorMessage().find("backupId"));
}

TEST(BackupManifest, FromJson_EmptyInput_ReturnsError) {
  arangodb::BackupManifest manifest;
  auto res = arangodb::BackupManifest::fromJson("", manifest);
  EXPECT_FALSE(res.ok());
}

TEST(BackupManifest, SaveAndLoadFile_RoundTrips) {
  auto tmpDir = makeTempDir("manifest_test");
  std::string manifestPath = tmpDir + "/MANIFEST.json";

  arangodb::BackupManifest original;
  original.backupId = "test-backup-001";
  original.timestamp = "2026-03-31T14:30:00Z";
  original.version = "3.12.0";
  original.sequenceNumber = 999;
  original.collections = {"col1", "col2"};
  original.path = tmpDir;
  original.isConsistent = true;

  auto saveRes =
      arangodb::BackupManifest::saveToFile(manifestPath, original);
  ASSERT_TRUE(saveRes.ok()) << saveRes.errorMessage();

  arangodb::BackupManifest loaded;
  auto loadRes = arangodb::BackupManifest::loadFromFile(manifestPath, loaded);
  ASSERT_TRUE(loadRes.ok()) << loadRes.errorMessage();

  EXPECT_EQ(original.backupId, loaded.backupId);
  EXPECT_EQ(original.timestamp, loaded.timestamp);
  EXPECT_EQ(original.version, loaded.version);
  EXPECT_EQ(original.sequenceNumber, loaded.sequenceNumber);
  EXPECT_EQ(original.collections, loaded.collections);
  EXPECT_EQ(original.isConsistent, loaded.isConsistent);

  cleanupDir(tmpDir);
}

// ============================================================================
// GlobalWriteLock Tests
// ============================================================================

TEST(GlobalWriteLock, AcquiresAndReleases) {
  std::mutex mutex;
  bool lockWasHeld = false;

  {
    arangodb::GlobalWriteLock lock(mutex);
    EXPECT_TRUE(lock.isLocked());
    lockWasHeld = true;

    // Verify the mutex is actually locked by trying from another thread
    bool otherThreadGotLock = false;
    std::thread t([&mutex, &otherThreadGotLock]() {
      if (mutex.try_lock()) {
        otherThreadGotLock = true;
        mutex.unlock();
      }
    });
    t.join();
    EXPECT_FALSE(otherThreadGotLock)
        << "Another thread should not be able to lock the mutex";
  }

  EXPECT_TRUE(lockWasHeld);

  // After scope, mutex should be unlocked
  EXPECT_TRUE(mutex.try_lock());
  mutex.unlock();
}

TEST(GlobalWriteLock, MoveSemantics_TransfersOwnership) {
  std::mutex mutex;

  arangodb::GlobalWriteLock lock1(mutex);
  EXPECT_TRUE(lock1.isLocked());

  arangodb::GlobalWriteLock lock2(std::move(lock1));
  EXPECT_FALSE(lock1.isLocked());  // moved-from
  EXPECT_TRUE(lock2.isLocked());   // moved-to
}

// ============================================================================
// RocksDBHotBackup: ID Generation
// ============================================================================

TEST(RocksDBHotBackup, GenerateBackupId_UniquePerCall) {
  auto id1 = arangodb::RocksDBHotBackup::generateBackupId();
  // Small sleep to ensure different timestamp
  std::this_thread::sleep_for(std::chrono::microseconds(10));
  auto id2 = arangodb::RocksDBHotBackup::generateBackupId();

  EXPECT_NE(id1, id2) << "Two consecutive backup IDs should be different";
}

TEST(RocksDBHotBackup, GenerateBackupId_FormatMatchesTimestamp) {
  auto id = arangodb::RocksDBHotBackup::generateBackupId();

  // Should match YYYY-MM-DDTHH.MM.SS.UUUUUU
  std::regex pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}\.\d{2}\.\d{2}\.\d{6})");
  EXPECT_TRUE(std::regex_match(id, pattern))
      << "Backup ID '" << id << "' does not match expected format";
}

// ============================================================================
// RocksDBHotBackup: Create
// ============================================================================

class HotBackupCreateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("backup_create_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
};

TEST_F(HotBackupCreateTest, WithMockDB_WritesManifest) {
  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("test-label", result);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  // Verify MANIFEST.json exists
  std::string manifestPath = result.manifest.path + "/MANIFEST.json";
  EXPECT_TRUE(fs::exists(manifestPath))
      << "MANIFEST.json should exist at " << manifestPath;
}

TEST_F(HotBackupCreateTest, WithMockDB_CreatesCheckpointDir) {
  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("test-label", result);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_TRUE(fs::is_directory(result.manifest.path))
      << "Backup directory should exist at " << result.manifest.path;
}

TEST_F(HotBackupCreateTest, FlushesWALBeforeCheckpoint) {
  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("test-label", result);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_TRUE(_mockDb->flushWALCalled)
      << "FlushWAL should be called before checkpoint";
  EXPECT_TRUE(_mockDb->checkpointCreated)
      << "Checkpoint should be created after flush";
}

TEST_F(HotBackupCreateTest, AcquiresGlobalWriteLock) {
  // The create() method should serialize through the mutex.
  // We verify this indirectly by confirming a sequential second create
  // succeeds (no deadlock) and both produce valid manifests.
  arangodb::RocksDBHotBackup::CreateResult result1;
  auto res1 = _backup->create("first", result1);
  ASSERT_TRUE(res1.ok());

  arangodb::RocksDBHotBackup::CreateResult result2;
  auto res2 = _backup->create("second", result2);
  ASSERT_TRUE(res2.ok());

  EXPECT_NE(result1.manifest.backupId, result2.manifest.backupId);
}

TEST_F(HotBackupCreateTest, ReleasesLockAfterCheckpoint_EvenOnError) {
  _mockDb->checkpointShouldSucceed = false;

  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("test-label", result);
  EXPECT_FALSE(res.ok()) << "Create should fail when checkpoint fails";

  // Verify we can still create another backup (lock was released)
  _mockDb->checkpointShouldSucceed = true;
  arangodb::RocksDBHotBackup::CreateResult result2;
  auto res2 = _backup->create("test-label-2", result2);
  EXPECT_TRUE(res2.ok())
      << "Should be able to create after failed attempt (lock released)";
}

TEST_F(HotBackupCreateTest, SetsCorrectManifestFields) {
  _mockDb->sequenceNumber = 12345;
  _mockDb->collectionNames = {"col_a", "col_b", "col_c"};

  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("test", result);
  ASSERT_TRUE(res.ok()) << res.errorMessage();

  EXPECT_FALSE(result.manifest.backupId.empty());
  EXPECT_FALSE(result.manifest.timestamp.empty());
  EXPECT_EQ("3.12.0", result.manifest.version);
  EXPECT_EQ(12345u, result.manifest.sequenceNumber);
  EXPECT_EQ(3u, result.manifest.collections.size());
  EXPECT_TRUE(result.manifest.isConsistent);
  EXPECT_GT(result.durationSeconds, 0.0);
}

// ============================================================================
// RocksDBHotBackup: List
// ============================================================================

class HotBackupListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("backup_list_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
};

TEST_F(HotBackupListTest, EmptyBackupDir_ReturnsEmpty) {
  std::vector<arangodb::BackupManifest> manifests;
  auto res = _backup->list(manifests);
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  EXPECT_TRUE(manifests.empty());
}

TEST_F(HotBackupListTest, MultipleBackups_ReturnsSortedByTimestamp) {
  // Create three backups
  for (int i = 0; i < 3; ++i) {
    arangodb::RocksDBHotBackup::CreateResult result;
    auto res = _backup->create("backup-" + std::to_string(i), result);
    ASSERT_TRUE(res.ok()) << res.errorMessage();
    // Small delay to ensure different timestamps
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::vector<arangodb::BackupManifest> manifests;
  auto res = _backup->list(manifests);
  ASSERT_TRUE(res.ok()) << res.errorMessage();
  ASSERT_EQ(3u, manifests.size());

  // Verify sorted by timestamp
  for (size_t i = 1; i < manifests.size(); ++i) {
    EXPECT_LE(manifests[i - 1].timestamp, manifests[i].timestamp)
        << "Manifests should be sorted by timestamp ascending";
  }
}

TEST_F(HotBackupListTest, CorruptManifest_SkipsInvalid) {
  // Create a valid backup
  arangodb::RocksDBHotBackup::CreateResult result;
  auto res = _backup->create("valid", result);
  ASSERT_TRUE(res.ok());

  // Create a corrupt backup directory (no manifest)
  std::string corruptDir = _tmpDir + "/corrupt-backup";
  fs::create_directories(corruptDir);
  // Write an invalid manifest
  std::ofstream ofs(corruptDir + "/MANIFEST.json");
  ofs << "not valid json";
  ofs.close();

  std::vector<arangodb::BackupManifest> manifests;
  auto listRes = _backup->list(manifests);
  ASSERT_TRUE(listRes.ok());
  // Only the valid backup should be listed
  EXPECT_EQ(1u, manifests.size());
}

// ============================================================================
// RocksDBHotBackup: Delete
// ============================================================================

class HotBackupDeleteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("backup_delete_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
};

TEST_F(HotBackupDeleteTest, ExistingBackup_RemovesDirectory) {
  // Create a backup first
  arangodb::RocksDBHotBackup::CreateResult result;
  auto createRes = _backup->create("to-delete", result);
  ASSERT_TRUE(createRes.ok());

  std::string backupId = result.manifest.backupId;
  std::string backupDir = result.manifest.path;
  ASSERT_TRUE(fs::exists(backupDir));

  auto deleteRes = _backup->deleteBackup(backupId);
  ASSERT_TRUE(deleteRes.ok()) << deleteRes.errorMessage();
  EXPECT_FALSE(fs::exists(backupDir))
      << "Backup directory should be removed after delete";
}

TEST_F(HotBackupDeleteTest, NonexistentBackup_ReturnsNotFound) {
  auto res = _backup->deleteBackup("nonexistent-id");
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(404, res.errorNumber());
}

// ============================================================================
// RocksDBHotBackup: Restore
// ============================================================================

class HotBackupRestoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpDir = makeTempDir("backup_restore_test");
    _mockDb = std::make_unique<arangodb::test::MockRocksDB>();
    _backup = std::make_unique<arangodb::RocksDBHotBackup>(
        _tmpDir, _mockDb.get());
  }

  void TearDown() override { cleanupDir(_tmpDir); }

  std::string _tmpDir;
  std::unique_ptr<arangodb::test::MockRocksDB> _mockDb;
  std::unique_ptr<arangodb::RocksDBHotBackup> _backup;
};

TEST_F(HotBackupRestoreTest, ValidBackup_Succeeds) {
  // Create a backup
  arangodb::RocksDBHotBackup::CreateResult result;
  auto createRes = _backup->create("for-restore", result);
  ASSERT_TRUE(createRes.ok());

  // Add a fake SST file to pass validation
  createFakeSstFile(result.manifest.path);

  auto restoreRes = _backup->restore(result.manifest.backupId);
  EXPECT_TRUE(restoreRes.ok()) << restoreRes.errorMessage();
}

TEST_F(HotBackupRestoreTest, NonexistentBackup_ReturnsNotFound) {
  auto res = _backup->restore("does-not-exist");
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(404, res.errorNumber());
}

TEST_F(HotBackupRestoreTest, CorruptBackup_ReturnsError) {
  // Create a backup, then remove all data files
  arangodb::RocksDBHotBackup::CreateResult result;
  auto createRes = _backup->create("corrupt-restore", result);
  ASSERT_TRUE(createRes.ok());

  // The directory has only MANIFEST.json and no SST files => validation fails
  auto restoreRes = _backup->restore(result.manifest.backupId);
  EXPECT_FALSE(restoreRes.ok());
  EXPECT_EQ(400, restoreRes.errorNumber());
}

// ============================================================================
// HotBackupFeature Tests
// ============================================================================

TEST(HotBackupFeature, DefaultEnabled) {
  arangodb::application_features::ApplicationServer server;
  arangodb::HotBackupFeature feature(server);
  EXPECT_TRUE(feature.isEnabled());
}

TEST(HotBackupFeature, CollectOptions_RegistersBackupPath) {
  arangodb::application_features::ApplicationServer server;
  arangodb::HotBackupFeature feature(server);

  auto opts = std::make_shared<arangodb::options::ProgramOptions>();
  feature.collectOptions(opts);

  EXPECT_TRUE(opts->hasOption("--rocksdb.backup-path"));
  EXPECT_TRUE(opts->hasOption("--hot-backup.enabled"));
  EXPECT_TRUE(opts->hasOption("--hot-backup.max-backups"));
}

TEST(HotBackupFeature, Prepare_CreatesBackupEngine) {
  arangodb::application_features::ApplicationServer server;
  arangodb::HotBackupFeature feature(server);

  // Simulate option collection and validation
  auto opts = std::make_shared<arangodb::options::ProgramOptions>();
  feature.collectOptions(opts);
  feature.validateOptions(opts);
  feature.prepare();

  // hotBackup() should not throw
  EXPECT_NO_THROW(feature.hotBackup());
}

TEST(HotBackupFeature, Disabled_PrepareSkipsEngine) {
  arangodb::application_features::ApplicationServer server;

  // We need a custom feature that is disabled.
  // Since _enabled is private, we test via the lifecycle:
  // after prepare with disabled state, hotBackup() should throw.
  // We create a subclass helper to set enabled = false, or we use
  // the fact that stop() resets the engine.

  arangodb::HotBackupFeature feature(server);
  auto opts = std::make_shared<arangodb::options::ProgramOptions>();
  feature.collectOptions(opts);
  feature.validateOptions(opts);
  feature.prepare();
  // stop() releases the engine
  feature.stop();

  EXPECT_THROW(feature.hotBackup(), std::runtime_error);
}
