#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::options {

class ProgramOptions {
 public:
  ProgramOptions() = default;
  virtual ~ProgramOptions() = default;

  // Stub: addOption for string vector (repeatable)
  void addOption(std::string const& name,
                 std::string const& /*description*/,
                 std::vector<std::string>& /*target*/) {
    _registeredOptions.push_back(name);
  }

  // Stub: addOption for bool
  void addOption(std::string const& name,
                 std::string const& /*description*/,
                 bool& /*target*/) {
    _registeredOptions.push_back(name);
  }

  // Stub: addOption for string
  void addOption(std::string const& name,
                 std::string const& /*description*/,
                 std::string& /*target*/) {
    _registeredOptions.push_back(name);
  }

  // Stub: addOption for uint64_t
  void addOption(std::string const& name,
                 std::string const& /*description*/,
                 uint64_t& /*target*/) {
    _registeredOptions.push_back(name);
  }

  // Test helper: check if an option was registered
  bool hasOption(std::string const& name) const {
    for (auto const& opt : _registeredOptions) {
      if (opt == name) return true;
    }
    return false;
  }

  std::vector<std::string> const& registeredOptions() const {
    return _registeredOptions;
  }

 private:
  std::vector<std::string> _registeredOptions;
};

}  // namespace arangodb::options
