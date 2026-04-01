#include "ChangelogBuffer.h"

#include <utility>

namespace arangodb {

namespace {
// Overhead per ChangelogEntry: opType (1) + 3 std::string objects overhead
// (typically 24 bytes each on 64-bit for small-string-optimized std::string)
// Plus the vector slot pointer overhead.
constexpr uint64_t kEntryBaseOverhead = 1 + 3 * 32 + 8;
}  // namespace

ChangelogBuffer::ChangelogBuffer(uint64_t memoryBudgetBytes)
    : _memoryBudgetBytes(memoryBudgetBytes),
      _currentMemoryUsage(0) {}

uint64_t ChangelogBuffer::estimateEntrySize(ChangelogEntry const& entry) {
  return kEntryBaseOverhead +
         entry.documentKey.capacity() +
         entry.documentRev.capacity() +
         entry.documentData.capacity();
}

bool ChangelogBuffer::append(ChangelogEntry entry) {
  uint64_t entrySize = estimateEntrySize(entry);

  std::lock_guard<std::mutex> lock(_mutex);

  if (_currentMemoryUsage + entrySize > _memoryBudgetBytes) {
    return false;
  }

  _currentMemoryUsage += entrySize;
  _entries.push_back(std::move(entry));
  return true;
}

void ChangelogBuffer::forEach(
    std::function<void(ChangelogEntry const&)> const& cb) const {
  std::lock_guard<std::mutex> lock(_mutex);
  for (auto const& entry : _entries) {
    cb(entry);
  }
}

uint64_t ChangelogBuffer::size() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _entries.size();
}

uint64_t ChangelogBuffer::memoryUsage() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _currentMemoryUsage;
}

void ChangelogBuffer::clear() {
  std::lock_guard<std::mutex> lock(_mutex);
  _entries.clear();
  _currentMemoryUsage = 0;
}

}  // namespace arangodb
