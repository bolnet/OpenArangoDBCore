#pragma once
#ifndef ARANGODB_TLS_CONNECTION_CONFIG_H
#define ARANGODB_TLS_CONNECTION_CONFIG_H

#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb {

/// Immutable configuration for an outbound mTLS connection.
/// All fields are validated at construction time; invalid configs
/// throw std::invalid_argument.
///
/// Used by DirectMQClient to initialize SSL_CTX.
struct TlsConnectionConfig {
  /// Construct and validate a TLS connection config.
  /// Throws std::invalid_argument if targetUrl, clientCertPath, or caPath
  /// is empty, or if minTlsVersion is not "1.2" or "1.3".
  TlsConnectionConfig(std::string targetUrl,
                       std::string clientCertPath,
                       std::string clientKeyPath,
                       std::string caPath,
                       std::string minTlsVersion = "1.2",
                       std::string cipherSuites = "");

  std::string const& targetUrl() const noexcept { return _targetUrl; }
  std::string const& clientCertPath() const noexcept { return _clientCertPath; }
  std::string const& clientKeyPath() const noexcept { return _clientKeyPath; }
  std::string const& caPath() const noexcept { return _caPath; }
  std::string const& minTlsVersion() const noexcept { return _minTlsVersion; }
  std::string const& cipherSuites() const noexcept { return _cipherSuites; }

 private:
  std::string _targetUrl;
  std::string _clientCertPath;
  std::string _clientKeyPath;
  std::string _caPath;
  std::string _minTlsVersion;
  std::string _cipherSuites;
};

}  // namespace arangodb

#endif  // ARANGODB_TLS_CONNECTION_CONFIG_H
