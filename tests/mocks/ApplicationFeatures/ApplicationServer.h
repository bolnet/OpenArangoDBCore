#pragma once
#include <cstddef>
#include <string_view>
#include <typeindex>

namespace arangodb::application_features {

class ApplicationFeature;

/// Mock ApplicationServer matching real ArangoDB v3.12.0 API.
/// In real ArangoDB, ApplicationServer is a plain class (no template).
/// Feature registration is runtime via addFeature<T>(), using
/// std::type_index for feature identity.
class ApplicationServer {
 public:
  ApplicationServer() = default;
  virtual ~ApplicationServer() = default;

  // Real API uses std::type_index, but for mock simplicity we use hash_code
  template<typename T>
  static constexpr size_t id() {
    return typeid(T).hash_code();
  }
};

}  // namespace arangodb::application_features

namespace arangodb {
// ArangodServer — in real ArangoDB this is a plain class inheriting
// ApplicationServer (defined in RestServer/arangod.h).
// No TypeList, no template — feature registration is runtime.
using ArangodServer = application_features::ApplicationServer;
}  // namespace arangodb
