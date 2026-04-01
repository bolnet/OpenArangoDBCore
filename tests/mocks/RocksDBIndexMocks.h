#pragma once
#ifndef ARANGODB_TESTS_ROCKSDB_INDEX_MOCKS_H
#define ARANGODB_TESTS_ROCKSDB_INDEX_MOCKS_H

#include "Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.h"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {
namespace test {

/// Mock iterator backed by an in-memory sorted map.
/// Simulates a RocksDB iterator bound to a snapshot.
class MockIndexIterator : public IndexIterator {
 public:
  using DataMap = std::map<std::string, std::string>;

  MockIndexIterator(std::shared_ptr<DataMap const> data,
                    std::string lowerBound,
                    std::string upperBound)
      : _data(std::move(data)),
        _lowerBound(std::move(lowerBound)),
        _upperBound(std::move(upperBound)),
        _current(_data->end()) {}

  void seek(std::string const& key) override {
    _current = _data->lower_bound(key);
  }

  bool valid() const override {
    if (_current == _data->end()) {
      return false;
    }
    if (!_upperBound.empty() && _current->first >= _upperBound) {
      return false;
    }
    return true;
  }

  void next() override {
    if (_current != _data->end()) {
      ++_current;
    }
  }

  std::string key() const override {
    return _current->first;
  }

  std::string value() const override {
    return _current->second;
  }

 private:
  std::shared_ptr<DataMap const> _data;
  std::string _lowerBound;
  std::string _upperBound;
  DataMap::const_iterator _current;
};

/// Factory that creates MockIndexIterator instances sharing the same
/// underlying data, simulating a shared MVCC snapshot.
class MockSnapshotFactory {
 public:
  explicit MockSnapshotFactory(MockIndexIterator::DataMap data)
      : _data(std::make_shared<MockIndexIterator::DataMap const>(
            std::move(data))),
        _createCount(0) {}

  std::unique_ptr<IndexIterator> create(std::string const& lowerBound,
                                        std::string const& upperBound) {
    std::lock_guard<std::mutex> lock(_mutex);
    ++_createCount;
    return std::make_unique<MockIndexIterator>(_data, lowerBound, upperBound);
  }

  /// How many iterators were created (i.e., how many threads requested one).
  uint32_t createCount() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _createCount;
  }

  /// Access the underlying data pointer for identity checks.
  void const* dataPointer() const {
    return _data.get();
  }

 private:
  std::shared_ptr<MockIndexIterator::DataMap const> _data;
  mutable std::mutex _mutex;
  uint32_t _createCount;
};

/// Thread-safe collector of inserted key-value pairs.
class MockIndexInserter {
 public:
  bool insert(std::string const& key, std::string const& value) {
    std::lock_guard<std::mutex> lock(_mutex);
    _inserted.emplace_back(key, value);
    return true;
  }

  std::vector<std::pair<std::string, std::string>> inserted() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _inserted;
  }

  size_t count() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _inserted.size();
  }

 private:
  mutable std::mutex _mutex;
  std::vector<std::pair<std::string, std::string>> _inserted;
};

}  // namespace test
}  // namespace arangodb

#endif  // ARANGODB_TESTS_ROCKSDB_INDEX_MOCKS_H
