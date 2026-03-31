#pragma once
#include <string_view>

namespace arangodb::application_features {

class ApplicationServer {
 public:
  ApplicationServer() = default;
  virtual ~ApplicationServer() = default;
};

}  // namespace arangodb::application_features
