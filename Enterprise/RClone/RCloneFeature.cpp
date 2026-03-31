#include "RCloneFeature.h"

#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"

static_assert(!std::is_abstract_v<arangodb::RCloneFeature>,
              "RCloneFeature must not be abstract");

namespace arangodb {

RCloneFeature::RCloneFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, *this) {}

}  // namespace arangodb
