#pragma once
/// Mock VelocyPack Builder for standalone testing.
/// Matches the real ArangoDB velocypack::Builder API surface used
/// by Enterprise code.

#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb::velocypack {

class Builder {
 public:
  Builder() = default;
  ~Builder() = default;

  // Object building
  Builder& openObject() { return *this; }
  Builder& close() { return *this; }
  Builder& add(std::string_view key, bool value) { return *this; }
  Builder& add(std::string_view key, int64_t value) { return *this; }
  Builder& add(std::string_view key, uint64_t value) { return *this; }
  Builder& add(std::string_view key, double value) { return *this; }
  Builder& add(std::string_view key, std::string_view value) { return *this; }
  Builder& add(std::string_view key, char const* value) { return *this; }

  // Array building
  Builder& openArray() { return *this; }
};

}  // namespace arangodb::velocypack

namespace arangodb {
using VPackBuilder = velocypack::Builder;
}  // namespace arangodb
