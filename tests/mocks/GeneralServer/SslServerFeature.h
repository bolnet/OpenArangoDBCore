#pragma once
/// Mock SslServerFeature for standalone builds.
/// Mirrors the virtual method interface of the real SslServerFeature
/// without any OpenSSL or asio dependencies.
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "velocypack/Builder.h"

#include <memory>
#include <string>
#include <vector>

namespace arangodb {

// Stub SslContextList type
using SslContextList = std::shared_ptr<std::vector<int>>;

class SslServerFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "SslServer";
  }

  explicit SslServerFeature(ArangodServer& server)
      : ArangodFeature(server, *this) {}
  virtual ~SslServerFeature() = default;

  // These are final in the real class
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
