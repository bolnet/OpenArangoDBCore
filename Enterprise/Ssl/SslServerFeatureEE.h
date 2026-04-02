#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "Enterprise/Basics/EnterpriseCompat.h"
#include "GeneralServer/SslServerFeature.h"

namespace arangodb {

/// Enterprise extension of SslServerFeature.
/// Registered via: addFeature<SslServerFeature, SslServerFeatureEE>()
/// which replaces the community SSL feature with this enterprise subclass.
///
/// Note: SslServerFeature::prepare() and unprepare() are `final`, so we
/// cannot override them here. The EE subclass only extends virtual methods
/// that are not marked final in the base.
///
/// Enterprise additions:
/// - mTLS client certificate authentication (--ssl.require-client-cert)
/// - TLS version minimum enforcement (--ssl.min-tls-version)
/// - Cipher suite allowlist (--ssl.enterprise-cipher-suites)
class SslServerFeatureEE final : public SslServerFeature {
 public:
  // Must match the name of the feature it replaces.
  static constexpr std::string_view name() noexcept { return "SslServer"; }

  explicit SslServerFeatureEE(ArangodServer& server);

  // Override to add enterprise options: mTLS, min TLS version, cipher suites.
  // MUST call SslServerFeature::collectOptions() first (Pitfall 6).
  void collectOptions(std::shared_ptr<options::ProgramOptions> opts) override;

  // Override to validate enterprise options.
  // MUST call SslServerFeature::validateOptions() first (Pitfall 6).
  void validateOptions(std::shared_ptr<options::ProgramOptions> opts) override;

  // Override to add enterprise validation (minimum TLS version enforcement).
  void verifySslOptions() override;

#if !defined(ARANGODB_INTEGRATION_BUILD) && !__has_include("RestServer/arangod.h")
  // Override to configure mTLS (client certificate verification) and
  // cipher suite restrictions.
  // Guarded: SslContextList is a mock type not available in integration mode.
  SslContextList createSslContexts() override;

  // Override to include enterprise TLS details in VPack dump.
  // Guarded: Result::success() is a mock API not available in integration mode.
  Result dumpTLSData(VPackBuilder& builder) const override;
#endif

  // Accessors for testing
  bool requireClientCert() const noexcept { return _requireClientCert; }
  std::string const& minTlsVersion() const noexcept { return _minTlsVersion; }
  std::string const& allowedCipherSuites() const noexcept {
    return _allowedCipherSuites;
  }

 private:
  bool _requireClientCert = false;       // --ssl.require-client-cert
  std::string _minTlsVersion = "1.2";   // --ssl.min-tls-version
  std::string _allowedCipherSuites;      // --ssl.enterprise-cipher-suites
};

}  // namespace arangodb
