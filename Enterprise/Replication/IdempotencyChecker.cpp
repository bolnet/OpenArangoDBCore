#include "IdempotencyChecker.h"

namespace arangodb {

IdempotencyChecker::IdempotencyChecker(SequenceNumberTracker& tracker)
    : _tracker(tracker) {}

CheckResult IdempotencyChecker::check(DirectMQMessage const& msg) const {
  if (_tracker.isAlreadyApplied(msg.shard_id, msg.sequence)) {
    return CheckResult::Reject;
  }
  // Check for gap: expected next is last_applied + 1.
  uint64_t lastApplied = _tracker.lastAppliedSequence(msg.shard_id);
  uint64_t expectedNext = lastApplied + 1;
  if (msg.sequence > expectedNext) {
    return CheckResult::OutOfOrder;
  }
  return CheckResult::Accept;
}

void IdempotencyChecker::accept(DirectMQMessage const& msg) {
  _tracker.markApplied(msg.shard_id, msg.sequence);
}

}  // namespace arangodb
