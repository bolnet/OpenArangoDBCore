#pragma once

namespace arangodb {

template <typename TrackerT>
CheckpointResult CheckpointReceiverT<TrackerT>::receive(
    std::string const& shardId, uint64_t appliedSequence) {
  // Look up current shard state
  auto state = _tracker.getState(shardId);

  // Validate shard exists (lastGenerated == 0 means unknown shard)
  if (state.lastGenerated == 0 && state.lastApplied == 0) {
    return CheckpointResult{false, "unknown shard: " + shardId};
  }

  // Validate sequence is advancing (not behind or equal to current)
  if (appliedSequence <= state.lastApplied) {
    return CheckpointResult{
        false,
        "appliedSequence " + std::to_string(appliedSequence) +
            " is not ahead of current lastApplied " +
            std::to_string(state.lastApplied) + " for shard " + shardId};
  }

  // Validate sequence is not ahead of what was generated
  if (appliedSequence > state.lastGenerated) {
    return CheckpointResult{
        false,
        "appliedSequence " + std::to_string(appliedSequence) +
            " exceeds lastGenerated " +
            std::to_string(state.lastGenerated) + " for shard " + shardId};
  }

  // Valid checkpoint -- update tracker
  _tracker.updateApplied(shardId, appliedSequence);
  return CheckpointResult{true, ""};
}

}  // namespace arangodb
