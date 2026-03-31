#pragma once
#ifndef ARANGODB_SSL_SERVER_FEATURE_EE_H
#define ARANGODB_SSL_SERVER_FEATURE_EE_H

#include "GeneralServer/SslServerFeature.h"

namespace arangodb {

class SslServerFeatureEE final : public SslServerFeature {
 public:
  static constexpr std::string_view name() noexcept { return "SslServer"; }
  explicit SslServerFeatureEE(application_features::ApplicationServer& server);
};

}  // namespace arangodb

#endif  // ARANGODB_SSL_SERVER_FEATURE_EE_H
