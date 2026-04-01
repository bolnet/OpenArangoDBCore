#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::options {

class ProgramOptions {
 public:
  ProgramOptions() = default;
  virtual ~ProgramOptions() = default;

  // addOption overloads for all types used by Enterprise features
  void addOption(std::string const& name, std::string const& /*desc*/,
                 std::string& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 bool& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 uint64_t& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 int64_t& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 uint32_t& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 int& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 double& /*target*/) { _opts.push_back(name); }
  void addOption(std::string const& name, std::string const& /*desc*/,
                 std::vector<std::string>& /*target*/) { _opts.push_back(name); }

  // Section management (no-op in mock)
  void addSection(std::string const& /*name*/, std::string const& /*desc*/) {}
  void addEnterpriseSection(std::string const& /*name*/,
                            std::string const& /*desc*/) {}

  // Test helpers
  bool hasOption(std::string const& name) const {
    for (auto const& opt : _opts) {
      if (opt == name) return true;
    }
    return false;
  }
  std::vector<std::string> const& registeredOptions() const { return _opts; }

 private:
  std::vector<std::string> _opts;
};

}  // namespace arangodb::options
