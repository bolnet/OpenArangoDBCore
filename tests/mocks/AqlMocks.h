#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace arangodb {
namespace aql {

/// Execution node type tags matching ArangoDB's ExecutionNode::NodeType
enum class ExecutionNodeType {
  ENUMERATE_IRESEARCH_VIEW,
  SORT,
  LIMIT,
  FILTER,
  RETURN,
  CALCULATION,
  OTHER
};

/// Minimal SortElement for testing optimizer rule pattern detection
struct SortElement {
  std::string attributePath;
  bool ascending;
};

/// Base class for mock execution nodes in a linked plan tree
class MockExecutionNode {
 public:
  explicit MockExecutionNode(ExecutionNodeType type) : _type(type) {}
  virtual ~MockExecutionNode() = default;

  ExecutionNodeType getType() const { return _type; }

  MockExecutionNode* getFirstDep() const { return _firstDep; }
  MockExecutionNode* getFirstParent() const { return _parent; }

  void addDependency(MockExecutionNode* dep) {
    _firstDep = dep;
    if (dep) {
      dep->setParent(this);
    }
  }

  void setParent(MockExecutionNode* parent) { _parent = parent; }

 private:
  ExecutionNodeType _type;
  MockExecutionNode* _firstDep = nullptr;
  MockExecutionNode* _parent = nullptr;
};

/// Mock EnumerateViewNode with WAND annotation fields
class MockEnumerateViewNode : public MockExecutionNode {
 public:
  MockEnumerateViewNode()
      : MockExecutionNode(ExecutionNodeType::ENUMERATE_IRESEARCH_VIEW) {}

  void setWandEnabled(bool enabled) { _enableWand = enabled; }
  void setWandHeapSize(size_t k) { _wandHeapSize = k; }
  bool wandEnabled() const { return _enableWand; }
  size_t wandHeapSize() const { return _wandHeapSize; }
  std::string const& viewName() const { return _viewName; }
  void setViewName(std::string name) { _viewName = std::move(name); }

 private:
  bool _enableWand = false;
  size_t _wandHeapSize = 0;
  std::string _viewName;
};

/// Mock SortNode exposing sort elements for pattern detection
class MockSortNode : public MockExecutionNode {
 public:
  MockSortNode() : MockExecutionNode(ExecutionNodeType::SORT) {}

  void addSortElement(std::string attributePath, bool ascending) {
    _elements.push_back({std::move(attributePath), ascending});
  }

  std::vector<SortElement> const& elements() const { return _elements; }

 private:
  std::vector<SortElement> _elements;
};

/// Mock LimitNode with limit and offset
class MockLimitNode : public MockExecutionNode {
 public:
  MockLimitNode() : MockExecutionNode(ExecutionNodeType::LIMIT) {}

  void setLimit(size_t limit) { _limit = limit; }
  void setOffset(size_t offset) { _offset = offset; }
  size_t limit() const { return _limit; }
  size_t offset() const { return _offset; }

 private:
  size_t _limit = 0;
  size_t _offset = 0;
};

}  // namespace aql
}  // namespace arangodb
