#pragma once
/// AQL type compatibility header for IResearch optimizer code.
///
/// In standalone mode, provides the mock AQL types.
/// In integration mode, includes the real AQL execution node types.

#ifdef ARANGODB_INTEGRATION_BUILD
// Real ArangoDB source tree
// In v3.12.0, LimitNode and EnumerateViewNode are defined within
// ExecutionNode.h or IResearchViewNode.h — not as separate headers
#include "Aql/ExecutionNode.h"
#include "Aql/SortNode.h"
#include "IResearch/IResearchViewNode.h"
#else
// Standalone build — use mock types
#include "AqlMocks.h"
#endif
