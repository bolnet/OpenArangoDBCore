#pragma once
/// Enterprise compatibility header.
///
/// In standalone mode (mock headers on include path):
///   ApplicationFeatures/ApplicationFeature.h provides ArangodFeature/ArangodServer
///
/// In integration mode (real ArangoDB source tree):
///   Forward-declare ArangodServer and provide the ArangodFeature alias
///   without pulling in the heavyweight RestServer/arangod.h header.

#ifdef ARANGODB_INTEGRATION_BUILD
// Real ArangoDB source tree — use the real ApplicationFeature framework
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"

// Forward declarations matching RestServer/arangod.h typedefs.
// The full definitions are available when linking against the arangod target.
namespace arangodb {
struct ArangodFeatures;
}
using ArangodServer = arangodb::application_features::ApplicationServerT<arangodb::ArangodFeatures>;
using ArangodFeature = arangodb::application_features::ApplicationFeatureT<ArangodServer>;

#else
// Standalone build — mocks provide ArangodFeature/ArangodServer
#include "ApplicationFeatures/ApplicationFeature.h"
#endif
