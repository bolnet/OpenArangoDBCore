#include "LicenseFeature.h"

#include <memory>
#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"

// Verify the vtable is fully satisfied — no pure virtual methods remain.
static_assert(!std::is_abstract_v<arangodb::LicenseFeature>,
              "LicenseFeature must not be abstract: check all pure virtual "
              "overrides in ApplicationFeature");

namespace arangodb {

LicenseFeature::LicenseFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, *this) {}

void LicenseFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> /*opts*/) {}

void LicenseFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*opts*/) {}

void LicenseFeature::prepare() {}

void LicenseFeature::start() {}

void LicenseFeature::beginShutdown() {}

void LicenseFeature::stop() {}

void LicenseFeature::unprepare() {}

}  // namespace arangodb
