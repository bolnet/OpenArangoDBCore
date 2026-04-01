// ReplicationFeedStream is a header-only template.
// This .cpp exists for CMake registration and to ensure the header compiles.
//
// Production instantiation would be:
//   ReplicationFeedStreamT<ShardWALTailer>
//
// Test instantiation uses:
//   ReplicationFeedStreamT<MockShardWALTailer>

#include "Enterprise/Replication/ReplicationFeedStream.h"
