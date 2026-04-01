// ReplicationStatusBuilder is a header-only template.
// This .cpp exists for CMake registration and to ensure the header compiles.
//
// Production instantiation would be:
//   ReplicationStatusBuilderT<SequenceNumberTracker, ShardWALTailer>
//
// Test instantiation uses:
//   ReplicationStatusBuilderT<MockSequenceNumberTracker, MockShardWALTailer>

#include "Enterprise/Replication/ReplicationStatusBuilder.h"
