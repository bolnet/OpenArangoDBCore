#include "TlsConnectionConfig.h"

#include <stdexcept>

namespace arangodb {

TlsConnectionConfig::TlsConnectionConfig(std::string targetUrl,
                                           std::string clientCertPath,
                                           std::string clientKeyPath,
                                           std::string caPath,
                                           std::string minTlsVersion,
                                           std::string cipherSuites)
    : _targetUrl(std::move(targetUrl)),
      _clientCertPath(std::move(clientCertPath)),
      _clientKeyPath(std::move(clientKeyPath)),
      _caPath(std::move(caPath)),
      _minTlsVersion(std::move(minTlsVersion)),
      _cipherSuites(std::move(cipherSuites)) {
  if (_targetUrl.empty()) {
    throw std::invalid_argument(
        "TlsConnectionConfig: targetUrl must not be empty");
  }
  if (_clientCertPath.empty()) {
    throw std::invalid_argument(
        "TlsConnectionConfig: clientCertPath must not be empty "
        "(mTLS requires a client certificate)");
  }
  if (_caPath.empty()) {
    throw std::invalid_argument(
        "TlsConnectionConfig: caPath must not be empty "
        "(server verification requires a CA certificate)");
  }
  if (_minTlsVersion != "1.2" && _minTlsVersion != "1.3") {
    throw std::invalid_argument(
        "TlsConnectionConfig: minTlsVersion must be '1.2' or '1.3', got: " +
        _minTlsVersion);
  }
}

}  // namespace arangodb
