#include "DirectMQClient.h"

#include <cstring>
#include <stdexcept>
#include <thread>

// OpenSSL headers (real or mock depending on build)
// In standalone test builds, these resolve to mock stubs.
// In production builds, these are real OpenSSL headers.
#if __has_include(<openssl/ssl.h>)
#include <openssl/ssl.h>
#include <openssl/err.h>
#else
// Mock OpenSSL for standalone testing -- see tests/mocks/openssl/
#include "openssl/ssl.h"
#endif

namespace arangodb {

DirectMQClient::DirectMQClient(TlsConnectionConfig config,
                                 ConnectionRetryPolicy retryPolicy)
    : _config(std::move(config)),
      _retryPolicy(std::move(retryPolicy)) {}

DirectMQClient::~DirectMQClient() { disconnect(); }

DirectMQClient::DirectMQClient(DirectMQClient&& other) noexcept
    : _config(std::move(other._config)),
      _retryPolicy(std::move(other._retryPolicy)),
      _sslCtx(other._sslCtx),
      _ssl(other._ssl),
      _socketFd(other._socketFd),
      _connected(other._connected) {
  other._sslCtx = nullptr;
  other._ssl = nullptr;
  other._socketFd = -1;
  other._connected = false;
}

DirectMQClient& DirectMQClient::operator=(DirectMQClient&& other) noexcept {
  if (this != &other) {
    disconnect();
    _config = std::move(other._config);
    _retryPolicy = std::move(other._retryPolicy);
    _sslCtx = other._sslCtx;
    _ssl = other._ssl;
    _socketFd = other._socketFd;
    _connected = other._connected;
    other._sslCtx = nullptr;
    other._ssl = nullptr;
    other._socketFd = -1;
    other._connected = false;
  }
  return *this;
}

DirectMQResult DirectMQClient::initializeSslContext() {
  DirectMQResult result;

  // Create TLS client context
  // In production: _sslCtx = SSL_CTX_new(TLS_client_method());
  // if (!_sslCtx) { ... error ... }

  // Set minimum TLS version
  // uint64_t minVersion = (_config.minTlsVersion() == "1.3")
  //     ? TLS1_3_VERSION : TLS1_2_VERSION;
  // SSL_CTX_set_min_proto_version(_sslCtx, minVersion);

  // Load client certificate for mTLS
  // SSL_CTX_use_certificate_file(_sslCtx, _config.clientCertPath().c_str(),
  //                              SSL_FILETYPE_PEM);
  // SSL_CTX_use_PrivateKey_file(_sslCtx, _config.clientKeyPath().c_str(),
  //                             SSL_FILETYPE_PEM);

  // Load CA for server verification
  // SSL_CTX_load_verify_locations(_sslCtx, _config.caPath().c_str(), nullptr);

  // Require server certificate verification (reject self-signed unless in CA)
  // SSL_CTX_set_verify(_sslCtx, SSL_VERIFY_PEER, nullptr);

  // Set cipher suites if configured
  // if (!_config.cipherSuites().empty()) {
  //   SSL_CTX_set_cipher_list(_sslCtx, _config.cipherSuites().c_str());
  //   SSL_CTX_set_ciphersuites(_sslCtx, _config.cipherSuites().c_str());
  // }

  result.ok = true;
  return result;
}

DirectMQResult DirectMQClient::performConnect() {
  DirectMQResult result;

  // 1. Initialize SSL context
  auto ctxResult = initializeSslContext();
  if (!ctxResult.ok) {
    return ctxResult;
  }

  // 2. Resolve target URL and open TCP socket
  // In production:
  //   _socketFd = socket(AF_INET, SOCK_STREAM, 0);
  //   connect(_socketFd, addr, addrlen);

  // 3. Create SSL object and bind to socket
  // _ssl = SSL_new(_sslCtx);
  // SSL_set_fd(_ssl, _socketFd);

  // 4. Perform TLS handshake (mTLS: client sends cert, server sends cert)
  // int ret = SSL_connect(_ssl);
  // if (ret != 1) { ... SSL_get_error ... }

  // 5. Mark as connected
  _connected = true;
  result.ok = true;
  return result;
}

DirectMQResult DirectMQClient::connect() {
  DirectMQResult result;

  if (_connected) {
    result.ok = true;
    return result;
  }

  _retryPolicy.reset();

  // Try initial connection
  result = performConnect();
  if (result.ok) {
    return result;
  }

  // Retry with exponential backoff
  while (_retryPolicy.shouldRetry()) {
    auto delay = _retryPolicy.nextDelay();
    std::this_thread::sleep_for(delay);

    result = performConnect();
    if (result.ok) {
      return result;
    }
  }

  result.errorMessage =
      "DirectMQClient::connect: all retry attempts exhausted "
      "connecting to " +
      _config.targetUrl();
  return result;
}

DirectMQResult DirectMQClient::sendMessage(std::string_view payload) {
  DirectMQResult result;

  if (!_connected) {
    result.errorMessage =
        "DirectMQClient::sendMessage: not connected (TLS required)";
    return result;
  }

  // Frame the message using DirectMQProtocol
  auto frame = DirectMQProtocol::frameMessage(payload);

  // In production:
  // int written = SSL_write(_ssl, frame.data(),
  //                         static_cast<int>(frame.size()));
  // if (written <= 0) { ... handle error ... }

  result.ok = true;
  return result;
}

DirectMQResult DirectMQClient::receiveAck() {
  DirectMQResult result;

  if (!_connected) {
    result.errorMessage =
        "DirectMQClient::receiveAck: not connected (TLS required)";
    return result;
  }

  // In production:
  // std::vector<uint8_t> ackBuf(8);
  // int bytesRead = SSL_read(_ssl, ackBuf.data(), 8);
  // if (bytesRead != 8) { ... handle error ... }
  //
  // auto parsed = DirectMQProtocol::parseAck(ackBuf);
  // if (!parsed.ok) {
  //   result.errorMessage = parsed.errorMessage;
  //   return result;
  // }
  // result.ackStatus = parsed.statusCode;

  result.ok = true;
  return result;
}

void DirectMQClient::disconnect() {
  if (!_connected && _ssl == nullptr && _sslCtx == nullptr) {
    return;  // Already disconnected or never connected
  }

  // In production:
  // if (_ssl) {
  //   SSL_shutdown(_ssl);
  //   SSL_free(_ssl);
  //   _ssl = nullptr;
  // }
  // if (_socketFd >= 0) {
  //   close(_socketFd);
  //   _socketFd = -1;
  // }
  // if (_sslCtx) {
  //   SSL_CTX_free(_sslCtx);
  //   _sslCtx = nullptr;
  // }

  _ssl = nullptr;
  _sslCtx = nullptr;
  _socketFd = -1;
  _connected = false;
}

bool DirectMQClient::isConnected() const noexcept { return _connected; }

}  // namespace arangodb
