#pragma once
/// Mock SslServerFeature for standalone builds.
/// Mirrors the virtual method interface of the real SslServerFeature
/// without any OpenSSL or asio dependencies.
#include "ApplicationFeatures/ApplicationFeature.h"

#include <memory>
#include <string>
#include <vector>

namespace arangodb {

// Mock VPackBuilder definition
namespace velocypack {
class Builder {
 public:
  Builder() = default;
  ~Builder() = default;
};
}  // namespace velocypack
using VPackBuilder = velocypack::Builder;

// Stub Result type
struct Result {
  bool ok() const { return _ok; }
  static Result success() { return Result{true}; }
  static Result failure() { return Result{false}; }
  bool _ok = true;
};

// Stub SslContextList type (normally std::shared_ptr<std::vector<asio_ns::ssl::context>>)
using SslContextList = std::shared_ptr<std::vector<int>>;  // placeholder

class SslServerFeature : public application_features::ApplicationFeature {
 public:
  explicit SslServerFeature(application_features::ApplicationServer& server)
      : ApplicationFeature(server, *this) {}
  virtual ~SslServerFeature() = default;

  // These are final in the real class -- cannot be overridden by EE
  void prepare() override final {}
  void unprepare() override final {}

  // Virtual methods available for EE override
  virtual void verifySslOptions() { _baseVerifyCalled = true; }
  virtual SslContextList createSslContexts() {
    _baseCreateCalled = true;
    return std::make_shared<std::vector<int>>();
  }
  virtual Result dumpTLSData(VPackBuilder& /*builder*/) const {
    return Result::success();
  }

  // Override points for options
  void collectOptions(std::shared_ptr<options::ProgramOptions> opts) override {
    _baseCollectCalled = true;
  }
  void validateOptions(std::shared_ptr<options::ProgramOptions> opts) override {
    _baseValidateCalled = true;
  }

  // Test inspection helpers
  bool _baseCollectCalled = false;
  bool _baseValidateCalled = false;
  bool _baseVerifyCalled = false;
  bool _baseCreateCalled = false;
};

}  // namespace arangodb
