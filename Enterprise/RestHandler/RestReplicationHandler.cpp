// RestReplicationHandler is a header-only template.
// This .cpp exists for CMake registration and to ensure the header compiles.
//
// Production instantiation would be:
//   RestReplicationHandlerT<SequenceNumberTracker, ShardWALTailer>
//
// Test instantiation uses:
//   RestReplicationHandlerT<MockSequenceNumberTracker, MockShardWALTailer>

#include "Enterprise/RestHandler/RestReplicationHandler.h"
