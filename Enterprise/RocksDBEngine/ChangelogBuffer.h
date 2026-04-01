#pragma once
#ifndef ARANGODB_CHANGELOG_BUFFER_H
#define ARANGODB_CHANGELOG_BUFFER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {

enum class ChangelogOpType : uint8_t {
  kInsert = 0,
  kUpdate = 1,
  kDelete = 2
};

struct ChangelogEntry {
  ChangelogOpType opType;
  std::string documentKey;
  std::string documentRev;
  std::string documentData;  // empty for deletes
};

class ChangelogBuffer {
 public:
  explicit ChangelogBuffer(uint64_t memoryBudgetBytes);

  /// Append an operation. Returns false if memory budget exceeded.
  bool append(ChangelogEntry entry);

  /// Iterate all entries in insertion order.
  void forEach(std::function<void(ChangelogEntry const&)> const& cb) const;

  /// Number of entries buffered.
  uint64_t size() const;

  /// Approximate memory usage in bytes.
  uint64_t memoryUsage() const;

  /// Remove all entries.
  void clear();

 private:
  /// Estimate memory used by a single entry (conservative).
  static uint64_t estimateEntrySize(ChangelogEntry const& entry);

  mutable std::mutex _mutex;
  std::vector<ChangelogEntry> _entries;
  uint64_t _memoryBudgetBytes;
  uint64_t _currentMemoryUsage;
};

}  // namespace arangodb
#endif
