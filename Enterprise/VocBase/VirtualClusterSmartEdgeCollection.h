#pragma once
#ifndef ARANGODB_VIRTUAL_CLUSTER_SMART_EDGE_COLLECTION_H
#define ARANGODB_VIRTUAL_CLUSTER_SMART_EDGE_COLLECTION_H

#include <string>
#include <vector>

namespace arangodb {

/// VirtualClusterSmartEdgeCollection wraps the internal sub-collections
/// (_local_E, _from_E, _to_E) of a SmartGraph edge collection as one logical
/// collection. In ArangoDB, smart edge data is physically split into three
/// sub-collections for routing efficiency:
///   _local_E: edges where both endpoints are on this shard
///   _from_E:  edges where _from is on this shard
///   _to_E:    edges where _to is on this shard
class VirtualClusterSmartEdgeCollection {
 public:
  explicit VirtualClusterSmartEdgeCollection(std::string name)
      : _name(std::move(name)),
        _localName(_name + "_local"),
        _fromName(_name + "_from"),
        _toName(_name + "_to") {}

  std::string const& name() const { return _name; }
  std::string const& localEdgeCollectionName() const { return _localName; }
  std::string const& fromEdgeCollectionName() const { return _fromName; }
  std::string const& toEdgeCollectionName() const { return _toName; }

  /// Returns all three sub-collection names.
  std::vector<std::string> subCollectionNames() const {
    return {_localName, _fromName, _toName};
  }

 private:
  std::string _name;
  std::string _localName;
  std::string _fromName;
  std::string _toName;
};

}  // namespace arangodb

#endif  // ARANGODB_VIRTUAL_CLUSTER_SMART_EDGE_COLLECTION_H
