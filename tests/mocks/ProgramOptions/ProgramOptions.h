#pragma once

#include <functional>
#include <string>
#include <vector>

namespace arangodb::options {

class ProgramOptions {
 public:
  ProgramOptions() = default;
  virtual ~ProgramOptions() = default;

  // Stub: addOption for string vector (repeatable)
  void addOption(std::string const& /*name*/,
                 std::string const& /*description*/,
                 std::vector<std::string>& /*target*/) {}
};

}  // namespace arangodb::options
