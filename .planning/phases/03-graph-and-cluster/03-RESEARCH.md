# Phase 3 Research: Graph & Cluster

**Researched:** 2026-03-31
**Confidence:** HIGH (direct source code inspection of ArangoDB devel branch)
**Scope:** SmartGraphs, Satellite Collections, Shard-local Execution, ReadFromFollower

---

## 1. SmartGraph Sharding Internals

### Sharding Strategy Hierarchy

```
ShardingStrategy (abstract base — arangod/Sharding/ShardingStrategy.h)
  ├── HashShardingStrategy ("hash")
  ├── CommunityCompat ("community-compat")
  └── [Enterprise only — registered via ShardingFeature::registerFactory()]
      ├── EnterpriseHashSmartEdge ("enterprise-hash-smart-edge")
      ├── EnterpriseHexSmartVertex ("enterprise-hex-smart-vertex")
      └── EnterpriseSmartEdgeCompat ("enterprise-smart-edge-compat")
```

### ShardingStrategy Pure Virtual Interface

```cpp
class ShardingStrategy {
public:
  virtual ~ShardingStrategy() = default;
  virtual std::string const& name() const = 0;
  virtual bool usesDefaultShardKeys() const = 0;
  virtual ResultT<ShardID> getResponsibleShard(
      velocypack::Slice slice,
      bool docComplete,
      bool& usesDefaultShardKeys,
      std::string_view key) = 0;
};
```

Registration: `ShardingFeature::registerFactory(name, factory)` in `prepare()`.
Community builds register stub factories that throw for enterprise strategy names.

### Strategy Selection Logic

| Collection Type | Strategy Name | Shard Key |
|---|---|---|
| Smart vertex WITH smartGraphAttribute | `"hash"` | Hashes on smartGraphAttribute value |
| Smart vertex WITHOUT smartGraphAttribute (EnterpriseGraph) | `"enterprise-hex-smart-vertex"` | Hex-encoded prefix |
| Smart edge collection (always) | `"enterprise-hash-smart-edge"` | Encodes both endpoints |

### _key Format Requirements

**Vertex keys:** `<smartGraphAttributeValue>:<localKey>`
- Example: `smartGraphAttribute = "region"`, `region: "DE"`, key `"111"` → `_key = "DE:111"`
- If user supplies a `_key` already containing `:`, no transformation occurs
- The prefix MUST match the document's `smartGraphAttribute` value

**Edge keys:** `<fromSmartValue>:<toSmartValue>:<edgeKey>`
- Encodes smartGraphAttribute of both `_from` and `_to` vertices
- Enables shard routing by examining `_key` alone without resolving `_from`/`_to`

### Collection Properties (from LogicalCollection.h)

```cpp
#ifdef USE_ENTERPRISE
  bool const _isDisjoint;
  bool const _isSmart;
  bool const _isSmartChild;
  std::string _smartGraphAttribute;
  std::string _smartJoinAttribute;
#endif
```

All getters return `false`/empty in community builds.

### Disjoint SmartGraph Enforcement

- All edges MUST connect vertices with the same `smartGraphAttribute` value
- Validated at insert/update time in `SmartGraphSchema`
- The `isDisjoint` flag stored on `GraphNode` structures in AQL plans
- Enables full query push-down to single DB-Server (40-120x speedup)

### distributeShardsLike Pattern

All collections in a SmartGraph must have the same `numberOfShards`.
First vertex collection defines sharding; all subsequent collections use
`distributeShardsLike` pointing to that initial collection.

---

## 2. Satellite Collection Distribution

### Configuration

- `replicationFactor: "satellite"` → internally stored as `0` in `_replicationFactor`
- `numberOfShards: 1` (always exactly one shard)
- `writeConcern` automatically equals number of DB-Servers (stored as `0`)

### Replication Mechanism

- Synchronous replication: writes execute on leader, replicated to ALL followers before ACK
- Every DB-Server holds a full replica
- Follower recovery: detects out-of-sync via Agency, auto-catches up

### SatelliteGraph

Named graph where ALL vertex and edge collections are SatelliteCollections.
Any DB-Server can execute the full traversal locally.

### CollectionAccess Satellite Tracking (arangod/Aql/CollectionAccess.h)

```cpp
class CollectionAccess {
  auto isUsedAsSatellite() const noexcept -> bool;
  void useAsSatelliteOf(ExecutionNodeId prototypeAccessId);
  auto getSatelliteOf(...) const -> ExecutionNode*;
private:
  mutable std::optional<ExecutionNodeId> _isSatelliteOf{std::nullopt};
};
```

### SmartToSat Edge Validation

When SmartGraph uses SatelliteCollections as vertex collections:
- Edge shard key derived from the SmartCollection vertex's smartGraphAttribute (not the Satellite side)
- `_from`/`_to` referencing a SatelliteCollection is permitted without matching smartGraphAttribute

---

## 3. Shard-local Graph Execution

### Node Hierarchy

```
ExecutionNode (abstract — arangod/Aql/ExecutionNode/ExecutionNode.h)
  └── GraphNode (arangod/Aql/ExecutionNode/GraphNode.h)
       ├── TraversalNode         (NodeType::TRAVERSAL = 22)
       ├── EnumeratePathsNode    (NodeType::ENUMERATE_PATHS = 25)  [virtual inheritance]
       └── ShortestPathNode      (NodeType::SHORTEST_PATH = 24)    [virtual inheritance]
```

### ExecutionNode Pure Virtual Methods

```cpp
virtual NodeType getType() const = 0;
virtual size_t getMemoryUsedBytes() const = 0;
virtual std::unique_ptr<ExecutionBlock> createBlock(ExecutionEngine&) const = 0;
virtual ExecutionNode* clone(ExecutionPlan*, bool withDependencies) const = 0;
virtual CostEstimate estimateCost() const = 0;
virtual void doToVelocyPack(velocypack::Builder&, unsigned flags) const = 0;
```

### GraphNode Enterprise-Gated Methods

```cpp
// Community stubs (return false/assert(false)):
bool isUsedAsSatellite() const;              // community: return false
bool isLocalGraphNode() const;               // community: return false
bool isHybridDisjoint() const;               // community: return false
void enableClusterOneShardRule(bool);         // community: TRI_ASSERT(false)
bool isClusterOneShardRuleEnabled() const;    // community: return false
```

### CRITICAL: LocalGraphNode Pattern

**There are NO separate NodeType enum values for local graph nodes.**

Local nodes reuse the same NodeType (TRAVERSAL, SHORTEST_PATH, ENUMERATE_PATHS) and
differentiate via `isLocalGraphNode` boolean in VelocyPack serialization:

```cpp
// From ExecutionNode.cpp deserialization:
case TRAVERSAL:
case SHORTEST_PATH:
case ENUMERATE_PATHS: {
  if (VelocyPackHelper::getBooleanValue(slice, "isLocalGraphNode", false)) {
    return createLocalGraphNode(plan, slice);  // enterprise-only factory
  }
  // standard node creation...
}
```

Community stub:
```cpp
#ifndef USE_ENTERPRISE
ExecutionNode* createLocalGraphNode(ExecutionPlan*, velocypack::Slice) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
      "Local graph nodes are available in ArangoDB Enterprise Edition only.");
}
#endif
```

### What OpenArangoDBCore Must Implement

1. **`createLocalGraphNode()`** factory — dispatch on NodeType in VelocyPack
2. **LocalTraversalNode** — extends TraversalNode, `isLocalGraphNode() = true`
3. **LocalEnumeratePathsNode** — extends EnumeratePathsNode, `isLocalGraphNode() = true`
4. **LocalShortestPathNode** — extends ShortestPathNode, `isLocalGraphNode() = true`
5. **LocalGraphNode** — possible shared base for all three
6. **GraphNode enterprise overrides** — `isUsedAsSatellite()`, `isLocalGraphNode()`, `enableClusterOneShardRule()`
7. Serialization: `doToVelocyPack()` must write `"isLocalGraphNode": true`

---

## 4. Graph Traversal Engine Architecture

### Provider Pattern (template-based, NOT virtual inheritance)

```
SingleServerProvider<StepImpl>  — local data access
ClusterProvider<StepImpl>       — distributed via engines
```

Duck-typed interface:
```cpp
startVertex(), fetchVertices(), fetchEdges(), expand()
createNeighbourCursor(), addVertexToBuilder(), addEdgeToBuilder()
stealStats(), prepareContext(), unPrepareContext()
isResponsible(), hasDepthSpecificLookup(), destroyEngines()
```

### SmartGraphProvider

Enterprise-only provider that routes traversal to correct shards.
Uses `TraverserEngine::smartSearchUnified()` for distributed smart search.

### SmartGraphStep

Enterprise step type that triggers conditional compilation in `OneSidedEnumerator`:
```cpp
using StepType = std::conditional_t<is_smart, enterprise::SmartGraphStep, SingleServerStep>;
```

### PathValidator Template

```cpp
template<class Provider, class PathStore,
         VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
class PathValidator {
  auto validatePath(Step& step) -> ValidationResult;
  auto validatePathUniqueness(Step& step) -> ValidationResult;
  // Enterprise: checkValidDisjointPath()
};
```

`PathValidatorOptions` includes `_isDisjoint`, `_isSatelliteLeader`, `_enabledClusterOneShardRule`.

### Enumerator Hierarchy

```
TraversalEnumerator (abstract base)
  ├── OneSidedEnumerator<Config>    — DFS/BFS/Weighted
  ├── TwoSidedEnumerator<P,V>      — bidirectional shortest paths
  ├── WeightedShortestPathEnumerator
  └── YenEnumerator
```

OneSidedEnumeratorEE.tpp extends OneSidedEnumerator for smart graph support.

### VirtualClusterSmartEdgeCollection

Wraps internal sub-collections (`_local_E`, `_from_E`, `_to_E`) as one logical edge collection.
Enterprise-only, accessed via `dynamic_cast<VirtualClusterSmartEdgeCollection*>` from `LogicalCollection`.
Enables operations like `count()` on distributed smart edge data.

---

## 5. ReadFromFollower

### Integration Point

```cpp
// ClusterInfo.h — enterprise method:
#ifdef USE_ENTERPRISE
void getResponsibleServersReadFromFollower(
    containers::FlatHashSet<ShardID> const& list,
    containers::FlatHashMap<ShardID, ServerID>& result);
#endif

// TransactionState.cpp — routing:
void TransactionState::chooseReplicasNolock(
    containers::FlatHashSet<ShardID> const& shards) {
#ifdef USE_ENTERPRISE
  ci.getResponsibleServersReadFromFollower(shards, *_chosenReplicas);
#else
  *_chosenReplicas = ci.getResponsibleServers(shards);
#endif
}
```

### HTTP Headers

- Request: `X-Arango-Allow-Dirty-Read: true` (StaticStrings::AllowDirtyReads)
- Response: `X-Arango-Potential-Dirty-Read: true`

### Behavior

- Coordinator routes reads to any replica (round-robin) when header present
- Only for read-only queries (no mutations)
- Stream Transactions: header on creation determines behavior for entire transaction
- In Active Failover: followers only serve reads with this header

---

## 6. Enterprise AQL Optimizer Rules

| Rule | Purpose |
|------|---------|
| `clusterOneShardRule` | Push entire query to single DB-Server (OneShard) |
| `smartJoinsRule` | Convert cross-shard joins to server-local |
| `scatterSatelliteGraphRule` | Remove scatter/gather for SatelliteCollection graphs |
| `removeSatelliteJoinsRule` | Remove scatter/gather for satellite joins |
| `removeDistributeNodesRule` | Remove unnecessary distribute/gather |
| `clusterLiftConstantsForDisjointGraphNodes` | Lift constants for disjoint SmartGraph traversals |
| `clusterPushSubqueryToDBServer` | Push subqueries to DB-Server |
| `skipInaccessibleCollectionsRule` | Replace inaccessible collection references |

---

## 7. Key Decisions for Implementation

1. **LocalGraphNode uses same NodeType** — no new enum values, differentiate via `isLocalGraphNode` flag
2. **Template-based providers** — SmartGraphProvider is a template instantiation, not virtual inheritance
3. **Factory function pattern** — `createLocalGraphNode()` must be implemented to replace community stub
4. **Sharding strategy registration** — via `ShardingFeature::registerFactory()` in `prepare()`
5. **_key format is the routing key** — all shard computation works from `_key` alone
6. **Satellite = replicationFactor 0** — internal representation, not "satellite" string
7. **ReadFromFollower is a routing change** — touches ClusterInfo and TransactionState

---

## Sources

- ArangoDB devel branch: ShardingStrategy.h, ExecutionNode.h, GraphNode.h, TraversalNode.h
- ArangoDB devel branch: CollectionAccess.h, ClusterInfo.h, TransactionState.cpp
- ArangoDB devel branch: ClusterProvider.h, TraverserEngine.h, OptimizerRules.h
- ArangoDB devel branch: Graph.h, LogicalCollection.h, ShardingInfo.h
- ArangoDB docs: SmartGraphs, SatelliteCollections, ReadFromFollower
- GitHub PR #11498: Disjoint SmartGraph query snippets
- GitHub PR #11512: Disjoint SmartGraph query fix
