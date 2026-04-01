#pragma once
#ifndef ARANGODB_MOCK_WAL_ITERATOR_H
#define ARANGODB_MOCK_WAL_ITERATOR_H

#include "Enterprise/Replication/IWALIterator.h"
#include <algorithm>
#include <vector>

namespace arangodb {
namespace test {

/// In-memory WAL iterator for unit tests.
/// Populate with entries, then iterate as if reading a real WAL.
class MockWALIterator final : public IWALIterator {
 public:
  MockWALIterator() = default;

  explicit MockWALIterator(std::vector<WALEntry> entries)
      : _entries(std::move(entries)), _pos(0) {}

  void addEntry(WALEntry entry) {
    _entries.push_back(std::move(entry));
  }

  bool valid() const override {
    return _pos < _entries.size();
  }

  WALEntry const& entry() const override {
    return _entries[_pos];
  }

  void next() override {
    if (_pos < _entries.size()) {
      ++_pos;
    }
  }

  void seek(uint64_t sequenceNumber) override {
    _pos = 0;
    while (_pos < _entries.size() &&
           _entries[_pos].sequenceNumber < sequenceNumber) {
      ++_pos;
    }
  }

  size_t size() const { return _entries.size(); }

 private:
  std::vector<WALEntry> _entries;
  size_t _pos = 0;
};

}  // namespace test
}  // namespace arangodb

#endif  // ARANGODB_MOCK_WAL_ITERATOR_H
