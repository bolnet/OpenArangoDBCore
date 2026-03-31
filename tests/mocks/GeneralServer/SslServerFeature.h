#pragma once
/// Mock SslServerFeature for standalone builds.
#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class SslServerFeature : public application_features::ApplicationFeature {
 public:
  explicit SslServerFeature(application_features::ApplicationServer& server)
      : ApplicationFeature(server, *this) {}
  virtual ~SslServerFeature() = default;
};

}  // namespace arangodb
