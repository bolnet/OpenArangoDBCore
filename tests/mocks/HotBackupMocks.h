#pragma once
/// Mock types for hot backup testing.
/// Provides a mock IRocksDBForBackup implementation that tracks calls
/// and a mock filesystem helper.

#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace arangodb::test {

/// Mock RocksDB database that tracks FlushWAL and Checkpoint calls.
class MockRocksDB : public arangodb::IRocksDBForBackup {
 public:
  bool flushWALCalled = false;
  bool checkpointCreated = false;
  std::string lastCheckpointPath;

  bool flushWALShouldSucceed = true;
  bool checkpointShouldSucceed = true;
  uint64_t sequenceNumber = 42;
  std::vector<std::string> collectionNames = {"_system", "_users"};

  bool flushWAL() override {
    flushWALCalled = true;
    return flushWALShouldSucceed;
  }

  bool createCheckpoint(std::string const& path) override {
    checkpointCreated = true;
    lastCheckpointPath = path;
    return checkpointShouldSucceed;
  }

  uint64_t getLatestSequenceNumber() const override { return sequenceNumber; }

  std::vector<std::string> getCollectionNames() const override {
    return collectionNames;
  }
};

/// Mock filesystem for backup directory operations (test helper).
class MockBackupFilesystem {
 public:
  std::set<std::string> directories;
  std::map<std::string, std::string> files;  // path -> content

  bool directoryExists(std::string const& path) const {
    return directories.count(path) > 0;
  }

  void createDirectory(std::string const& path) { directories.insert(path); }

  void writeFile(std::string const& path, std::string const& content) {
    files[path] = content;
  }

  std::string readFile(std::string const& path) const {
    auto it = files.find(path);
    if (it != files.end()) {
      return it->second;
    }
    return "";
  }
};

}  // namespace arangodb::test
