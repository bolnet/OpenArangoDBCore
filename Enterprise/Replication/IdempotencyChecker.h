#pragma once
#ifndef ARANGODB_IDEMPOTENCY_CHECKER_H
#define ARANGODB_IDEMPOTENCY_CHECKER_H

#include "DirectMQMessage.h"
#include "SequenceNumberTracker.h"

namespace arangodb {

/// Result of an idempotency check on an incoming replication message.
enum class CheckResult : uint8_t {
  Accept = 0,      // Message is new and in-order; safe to apply.
  Reject = 1,      // Message is a duplicate (seq <= last_applied).
  OutOfOrder = 2   // Message is new but skipped sequences (gap detected).
};

/// Validates incoming DirectMQ messages against a SequenceNumberTracker
/// to enforce idempotent replay semantics.
///
/// Usage:
///   IdempotencyChecker checker(tracker);
///   auto result = checker.check(message);
///   if (result == CheckResult::Accept || result == CheckResult::OutOfOrder) {
///     applyMessage(message);
///     checker.accept(message);
///   }
class IdempotencyChecker {
 public:
  /// Construct with a reference to the sequence tracker.
  /// The tracker must outlive the checker.
  explicit IdempotencyChecker(SequenceNumberTracker& tracker);

  /// Check whether a message should be applied.
  /// Does NOT modify tracker state (read-only check).
  CheckResult check(DirectMQMessage const& msg) const;

  /// Record that a message was successfully applied.
  /// Updates the tracker with the message's sequence number.
  void accept(DirectMQMessage const& msg);

 private:
  SequenceNumberTracker& _tracker;
};

}  // namespace arangodb

#endif  // ARANGODB_IDEMPOTENCY_CHECKER_H
