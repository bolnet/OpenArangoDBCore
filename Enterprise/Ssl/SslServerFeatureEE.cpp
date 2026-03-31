#include "SslServerFeatureEE.h"

#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"

static_assert(!std::is_abstract_v<arangodb::SslServerFeatureEE>,
              "SslServerFeatureEE must not be abstract");

namespace arangodb {

SslServerFeatureEE::SslServerFeatureEE(
    application_features::ApplicationServer& server)
    : SslServerFeature(server) {}

}  // namespace arangodb
