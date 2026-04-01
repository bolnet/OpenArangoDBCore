#pragma once
#ifndef ARANGODB_IWAL_ITERATOR_H
#define ARANGODB_IWAL_ITERATOR_H

#include <cstdint>
#include <string>

namespace arangodb {

/// A single entry read from the RocksDB WAL.
struct WALEntry {
  uint64_t sequenceNumber;    // RocksDB WAL sequence
  uint64_t timestamp;         // Microseconds since epoch
  std::string collectionName; // Source collection
  std::string documentKey;    // Document _key
  enum class Operation : uint8_t { kInsert = 0, kUpdate = 1, kDelete = 2 };
  Operation operation;
  std::string payload;        // VPack-encoded body (empty for deletes)
};

/// Abstract iterator over WAL entries.
/// Implementations: RocksDB WAL reader (production), MockWALIterator (test).
class IWALIterator {
 public:
  virtual ~IWALIterator() = default;

  /// True if the iterator points to a valid entry.
  virtual bool valid() const = 0;

  /// Access the current entry. UB if !valid().
  virtual WALEntry const& entry() const = 0;

  /// Advance to the next entry.
  virtual void next() = 0;

  /// Seek to the first entry with sequenceNumber >= target.
  virtual void seek(uint64_t sequenceNumber) = 0;
};

}  // namespace arangodb

#endif  // ARANGODB_IWAL_ITERATOR_H
