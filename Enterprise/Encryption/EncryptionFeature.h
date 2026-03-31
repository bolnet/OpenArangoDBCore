#pragma once

#include <memory>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

namespace options {
class ProgramOptions;
}

class EncryptionFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Encryption"; }

  explicit EncryptionFeature(application_features::ApplicationServer& server)
      : ApplicationFeature(server, *this) {}

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override {}
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override {}
  void prepare() override {}
  void start() override {}
  void beginShutdown() override {}
  void stop() override {}
  void unprepare() override {}
};

}  // namespace arangodb
