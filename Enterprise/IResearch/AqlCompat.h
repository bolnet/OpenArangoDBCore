#pragma once
/// AQL type compatibility header for IResearch optimizer code.
///
/// In standalone mode, provides the mock AQL types.
/// In integration mode, includes the real AQL execution node types.

#ifdef ARANGODB_INTEGRATION_BUILD
// Real ArangoDB source tree
#include "Aql/ExecutionNode.h"
#include "Aql/SortNode.h"
#include "Aql/LimitNode.h"
#include "Aql/EnumerateViewNode.h"
#else
// Standalone build — use mock types
#include "AqlMocks.h"
#endif
