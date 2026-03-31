#pragma once
/// Mock VelocyPack Builder for standalone testing.

namespace arangodb::velocypack {

class Builder {
 public:
  Builder() = default;
  ~Builder() = default;
};

}  // namespace arangodb::velocypack
