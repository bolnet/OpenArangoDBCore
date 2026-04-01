#pragma once
#include <cstddef>
#include <string_view>

namespace arangodb::application_features {

class ApplicationFeature;

class ApplicationServer {
 public:
  ApplicationServer() = default;
  virtual ~ApplicationServer() = default;

  // Mock: static feature ID registry (returns hash of type for uniqueness)
  template<typename T>
  static constexpr size_t id() {
    // Use a simple compile-time hash based on feature name
    return typeid(T).hash_code();
  }
};

// Typed server template matching real ArangoDB pattern
template<typename... Features>
class ApplicationServerT : public ApplicationServer {
 public:
  ApplicationServerT() = default;

  template<typename T>
  static constexpr size_t id() {
    return ApplicationServer::id<T>();
  }
};

}  // namespace arangodb::application_features

namespace arangodb {
// ArangodServer type alias — in real ArangoDB this is
// ApplicationServerT<ArangodFeaturesList> but for mocks we use a simple alias
using ArangodServer = application_features::ApplicationServer;
}  // namespace arangodb
