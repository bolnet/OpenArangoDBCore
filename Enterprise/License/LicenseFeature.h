#pragma once

#include <memory>
#include <string_view>

#include "Enterprise/Basics/EnterpriseCompat.h"

namespace arangodb {

namespace options {
class ProgramOptions;
}

class LicenseFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "License"; }

  explicit LicenseFeature(ArangodServer& server);

  // RestLicenseHandler calls this.
  // Return false = open-source behavior (no super-user restriction).
  bool onlySuperUser() const noexcept { return false; }

  // Enterprise capability check — always return true.
  // Open-source build unlocks all enterprise features.
  bool isEnterprise() const noexcept { return true; }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;
};

}  // namespace arangodb
