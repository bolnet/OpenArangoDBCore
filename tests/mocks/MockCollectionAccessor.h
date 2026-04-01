#pragma once
#ifndef ARANGODB_TEST_MOCK_COLLECTION_ACCESSOR_H
#define ARANGODB_TEST_MOCK_COLLECTION_ACCESSOR_H

#include "Enterprise/Replication/ReplicationApplier.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arangodb {
namespace test {

/// In-memory mock collection store for integration tests.
/// Tracks applied operations per shard, supporting INSERT, UPDATE, REMOVE,
/// and TRUNCATE semantics.
class MockCollectionStore {
 public:
  /// Execute a write operation against the mock store.
  int write(std::string const& shard, ReplicationOperation op,
            std::vector<uint8_t> const& payload, std::string const& docKey) {
    std::lock_guard<std::mutex> guard(_mutex);

    if (_failOnWrite) {
      return _failErrorCode;
    }

    switch (op) {
      case ReplicationOperation::INSERT: {
        _documents[shard].push_back({docKey, payload});
        _keys[shard].insert(docKey);
        break;
      }
      case ReplicationOperation::UPDATE: {
        // Remove old version if exists, then insert new.
        auto& docs = _documents[shard];
        docs.erase(
            std::remove_if(docs.begin(), docs.end(),
                           [&](auto const& p) { return p.first == docKey; }),
            docs.end());
        docs.push_back({docKey, payload});
        _keys[shard].insert(docKey);
        break;
      }
      case ReplicationOperation::REMOVE: {
        auto& docs = _documents[shard];
        docs.erase(
            std::remove_if(docs.begin(), docs.end(),
                           [&](auto const& p) { return p.first == docKey; }),
            docs.end());
        _keys[shard].erase(docKey);
        break;
      }
      case ReplicationOperation::TRUNCATE: {
        _documents[shard].clear();
        _keys[shard].clear();
        break;
      }
    }
    ++_writeCount;
    return 0;
  }

  /// Get document count for a shard.
  uint64_t documentCount(std::string const& shard) const {
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _documents.find(shard);
    if (it == _documents.end()) {
      return 0;
    }
    return it->second.size();
  }

  /// Check if a document exists in a shard.
  bool hasDocument(std::string const& shard, std::string const& key) const {
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _keys.find(shard);
    if (it == _keys.end()) {
      return false;
    }
    return it->second.count(key) > 0;
  }

  /// Get total write operations executed.
  uint64_t writeCount() const {
    std::lock_guard<std::mutex> guard(_mutex);
    return _writeCount;
  }

  /// Configure the store to fail writes with a specific error code.
  void setFailOnWrite(bool fail, int errorCode = 99) {
    std::lock_guard<std::mutex> guard(_mutex);
    _failOnWrite = fail;
    _failErrorCode = errorCode;
  }

  /// Create a WriteCallback bound to this store.
  WriteCallback makeWriteCallback() {
    return [this](std::string const& shard, ReplicationOperation op,
                  std::vector<uint8_t> const& payload,
                  std::string const& docKey) -> int {
      return this->write(shard, op, payload, docKey);
    };
  }

 private:
  mutable std::mutex _mutex;
  // shard -> [(docKey, payload), ...]
  std::unordered_map<std::string,
                     std::vector<std::pair<std::string, std::vector<uint8_t>>>>
      _documents;
  // shard -> set of keys (for fast lookup)
  std::unordered_map<std::string, std::unordered_set<std::string>> _keys;
  uint64_t _writeCount{0};
  bool _failOnWrite{false};
  int _failErrorCode{99};
};

/// Simulates network partition by dropping messages in a sequence range.
class PartitionSimulator {
 public:
  PartitionSimulator(uint64_t dropFrom, uint64_t dropTo)
      : _dropFrom(dropFrom), _dropTo(dropTo) {}

  bool shouldDrop(uint64_t sequence) const {
    return sequence >= _dropFrom && sequence <= _dropTo;
  }

  std::vector<ApplyMessage> filter(
      std::vector<ApplyMessage> const& batch) const {
    std::vector<ApplyMessage> passed;
    for (auto const& msg : batch) {
      if (shouldDrop(msg.sequence)) {
        _dropped.push_back(msg);
      } else {
        passed.push_back(msg);
      }
    }
    return passed;
  }

  std::vector<ApplyMessage> getDropped() const { return _dropped; }

  uint64_t droppedCount() const { return _dropped.size(); }

 private:
  uint64_t _dropFrom;
  uint64_t _dropTo;
  mutable std::vector<ApplyMessage> _dropped;
};

}  // namespace test
}  // namespace arangodb

#endif  // ARANGODB_TEST_MOCK_COLLECTION_ACCESSOR_H
