#pragma once
/// Enterprise compatibility header.
///
/// In standalone mode (mock headers on include path):
///   ApplicationFeatures/ApplicationFeature.h provides ArangodFeature/ArangodServer
///
/// In integration mode (real ArangoDB source tree):
///   Include the real ApplicationFeature framework and RestServer/arangod.h
///   which defines ArangodServer as a plain class (not a TypeList template).

// Auto-detect: if RestServer/arangod.h is on the include path, we're inside
// the real ArangoDB source tree. This works regardless of which CMake target
// compiles the translation unit (our enterprise target OR arangod's own targets).
#if defined(ARANGODB_INTEGRATION_BUILD) || __has_include("RestServer/arangod.h")
// Real ArangoDB source tree — ArangodServer is a plain class inheriting
// ApplicationServer.  Feature registration is runtime (addFeature<T>()),
// not compile-time TypeList.
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/arangod.h"

#else
// Standalone build — mocks provide ArangodFeature/ArangodServer
#include "ApplicationFeatures/ApplicationFeature.h"
#endif
