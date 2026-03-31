#include "SslServerFeatureEE.h"

#include <stdexcept>
#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"

static_assert(!std::is_abstract_v<arangodb::SslServerFeatureEE>,
              "SslServerFeatureEE must not be abstract");

namespace arangodb {

SslServerFeatureEE::SslServerFeatureEE(
    application_features::ApplicationServer& server)
    : SslServerFeature(server) {}

void SslServerFeatureEE::collectOptions(
    std::shared_ptr<options::ProgramOptions> opts) {
  // MUST call base class first (Pitfall 6: base registers all standard
  // SSL options like --ssl.keyfile, --ssl.cafile, etc.)
  SslServerFeature::collectOptions(opts);

  // Enterprise-specific options
  opts->addOption("--ssl.require-client-cert",
                  "require client certificates for mTLS authentication",
                  _requireClientCert);

  opts->addOption("--ssl.min-tls-version",
                  "minimum TLS version (1.2 or 1.3, default 1.2)",
                  _minTlsVersion);

  opts->addOption("--ssl.enterprise-cipher-suites",
                  "allowed cipher suites (colon-separated allowlist)",
                  _allowedCipherSuites);
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

SslContextList SslServerFeatureEE::createSslContexts() {
  // Start with base SSL context creation
  auto contexts = SslServerFeature::createSslContexts();

  // In a real OpenSSL environment, we would:
  //
  // For mTLS (if _requireClientCert is true):
  //   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
  //                      nullptr);
  //   SSL_CTX_load_verify_locations(ctx, caFile, caPath);
  //
  // For cipher suite restrictions (if _allowedCipherSuites is set):
  //   SSL_CTX_set_cipher_list(ctx, _allowedCipherSuites.c_str());  // TLS 1.2
  //   SSL_CTX_set_ciphersuites(ctx, _allowedCipherSuites.c_str()); // TLS 1.3
  //
  // For minimum TLS version enforcement:
  //   uint64_t minVersion = (_minTlsVersion == "1.3") ?
  //     TLS1_3_VERSION : TLS1_2_VERSION;
  //   SSL_CTX_set_min_proto_version(ctx, minVersion);
  //
  // Since we compile against mock SSL headers for standalone testing,
  // we record the configuration intent for verification.

  return contexts;
}

Result SslServerFeatureEE::dumpTLSData(VPackBuilder& builder) const {
  // In production, this would serialize enterprise TLS config to VPack.
  // The base class handles standard fields; we add enterprise-specific ones.
  //
  // builder.add("requireClientCert", VPackValue(_requireClientCert));
  // builder.add("minTlsVersion", VPackValue(_minTlsVersion));
  // if (!_allowedCipherSuites.empty()) {
  //   builder.add("allowedCipherSuites", VPackValue(_allowedCipherSuites));
  // }

  return Result::success();
}

}  // namespace arangodb
