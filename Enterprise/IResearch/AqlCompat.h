#pragma once
/// AQL type compatibility header for IResearch optimizer code.
///
/// In standalone mode, provides the mock AQL types.
/// In integration mode, includes the real AQL execution node types.

#ifdef ARANGODB_INTEGRATION_BUILD
// Real ArangoDB source tree — ExecutionNode.h defines the base types.
// Our TopK optimizer uses mock types (MockSortNode etc.) which are
// only used in standalone mode. In integration mode, the real optimizer
// lives in arangod and our code is not compiled into the TopK path.
#include "Aql/ExecutionNode.h"
#else
// Standalone build — use mock types
#include "AqlMocks.h"
#endif
