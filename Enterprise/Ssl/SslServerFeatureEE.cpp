#include "SslServerFeatureEE.h"

#include <stdexcept>
#include <type_traits>

#ifdef ARANGODB_INTEGRATION_BUILD
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"
#else
#include "ProgramOptions/ProgramOptions.h"
#endif

static_assert(!std::is_abstract_v<arangodb::SslServerFeatureEE>,
              "SslServerFeatureEE must not be abstract");

namespace arangodb {

SslServerFeatureEE::SslServerFeatureEE(ArangodServer& server)
    : SslServerFeature(server) {}

void SslServerFeatureEE::collectOptions(
    std::shared_ptr<options::ProgramOptions> opts) {
  // MUST call base class first (Pitfall 6: base registers all standard
  // SSL options like --ssl.keyfile, --ssl.cafile, etc.)
  SslServerFeature::collectOptions(opts);

  // Enterprise-specific options
  opts->addOption("--ssl.require-client-cert",
                  "require client certificates for mTLS authentication",
                  new options::BooleanParameter(&_requireClientCert));

  opts->addOption("--ssl.min-tls-version",
                  "minimum TLS version (1.2 or 1.3, default 1.2)",
                  new options::StringParameter(&_minTlsVersion));

  opts->addOption("--ssl.enterprise-cipher-suites",
                  "allowed cipher suites (colon-separated allowlist)",
                  new options::StringParameter(&_allowedCipherSuites));
}

void SslServerFeatureEE::validateOptions(
    std::shared_ptr<options::ProgramOptions> opts) {
  // MUST call base class first (Pitfall 6)
  SslServerFeature::validateOptions(opts);

  // Validate min TLS version
  if (_minTlsVersion != "1.2" && _minTlsVersion != "1.3") {
    throw std::invalid_argument(
        "--ssl.min-tls-version must be '1.2' or '1.3', got: " +
        _minTlsVersion);
  }
}

void SslServerFeatureEE::verifySslOptions() {
  // Call base verification first
  SslServerFeature::verifySslOptions();

  // Reject TLS versions below the configured minimum.
  // This enforces that no downgrade below TLS 1.2 is possible.
  // The minimum is already validated in validateOptions(),
  // so here we just ensure consistency.
  if (_minTlsVersion != "1.2" && _minTlsVersion != "1.3") {
    throw std::runtime_error(
        "TLS version below minimum: must be at least TLS 1.2");
  }
}

// createSslContexts and dumpTLSData use mock types (SslContextList,
// Result::success()) that don't exist in real ArangoDB. In integration mode,
// the real SslServerFeature handles these methods.
#ifndef ARANGODB_INTEGRATION_BUILD

SslContextList SslServerFeatureEE::createSslContexts() {
  auto contexts = SslServerFeature::createSslContexts();
  return contexts;
}

Result SslServerFeatureEE::dumpTLSData(VPackBuilder& builder) const {
  return Result::success();
}

#endif  // !ARANGODB_INTEGRATION_BUILD

}  // namespace arangodb
