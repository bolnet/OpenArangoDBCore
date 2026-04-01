// CheckpointReceiver is a header-only template.
// This .cpp exists for CMake registration and to ensure the header compiles.
//
// Production instantiation would be:
//   CheckpointReceiverT<SequenceNumberTracker>
//
// Test instantiation uses:
//   CheckpointReceiverT<MockSequenceNumberTracker>

#include "Enterprise/Replication/CheckpointReceiver.h"
