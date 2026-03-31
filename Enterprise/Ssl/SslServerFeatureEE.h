#pragma once

#include <string_view>

#include "GeneralServer/SslServerFeature.h"

namespace arangodb {

// Enterprise extension of SslServerFeature.
// Registered via: addFeature<SslServerFeature, SslServerFeatureEE>()
// which replaces the community SSL feature with this enterprise subclass.
//
// Note: SslServerFeature::prepare() and unprepare() are `final`, so we
// cannot override them here. The EE subclass only extends virtual methods
// that are not marked final in the base.
class SslServerFeatureEE final : public SslServerFeature {
 public:
  // Must match the name of the feature it replaces.
  static constexpr std::string_view name() noexcept { return "SslServer"; }

  explicit SslServerFeatureEE(application_features::ApplicationServer& server);
};

}  // namespace arangodb
