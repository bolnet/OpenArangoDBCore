#pragma once
#ifndef ARANGODB_REPLICATION_CHECKPOINT_H
#define ARANGODB_REPLICATION_CHECKPOINT_H

#include "SequenceNumberTracker.h"
#include <string>

namespace arangodb {

/// Result of a checkpoint operation.
struct CheckpointResult {
  bool success = false;
  std::string error;  // Empty on success.
};

/// Persists SequenceNumberTracker state to disk as JSON.
///
/// File format:
/// {
///   "version": 1,
///   "shards": {
///     "s10001": 42,
///     "s10002": 17
///   }
/// }
///
/// Used for crash recovery: on startup, the target loads the last
/// checkpoint to resume replication without re-applying all messages.
class ReplicationCheckpoint {
 public:
  /// Construct with file path and tracker reference.
  ReplicationCheckpoint(std::string path, SequenceNumberTracker& tracker);

  /// Save current tracker state to disk.
  /// Writes to a temporary file then renames for atomicity.
  CheckpointResult save() const;

  /// Load tracker state from disk.
  /// If file does not exist, returns success with empty state (fresh start).
  CheckpointResult load();

  /// Return the configured checkpoint file path.
  std::string const& path() const { return _path; }

 private:
  std::string _path;
  SequenceNumberTracker& _tracker;
};

}  // namespace arangodb

#endif  // ARANGODB_REPLICATION_CHECKPOINT_H
