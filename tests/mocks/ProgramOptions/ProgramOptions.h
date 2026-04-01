#pragma once
/// Mock ProgramOptions for standalone builds.
/// Supports both the real API pattern (Parameter* objects) and
/// direct-reference convenience overloads for simpler test code.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::options {

// ---------------------------------------------------------------------------
// Mock Parameter hierarchy (matches real ArangoDB ProgramOptions/Parameters.h)
// ---------------------------------------------------------------------------
class Parameter {
 public:
  virtual ~Parameter() = default;
};

class StringParameter : public Parameter {
 public:
  explicit StringParameter(std::string* /*target*/) {}
};

class BooleanParameter : public Parameter {
 public:
  explicit BooleanParameter(bool* /*target*/) {}
};

class UInt64Parameter : public Parameter {
 public:
  explicit UInt64Parameter(uint64_t* /*target*/) {}
};

class Int64Parameter : public Parameter {
 public:
  explicit Int64Parameter(int64_t* /*target*/) {}
};

class UInt32Parameter : public Parameter {
 public:
  explicit UInt32Parameter(uint32_t* /*target*/) {}
};

class Int32Parameter : public Parameter {
 public:
  explicit Int32Parameter(int* /*target*/) {}
};

class DoubleParameter : public Parameter {
 public:
  explicit DoubleParameter(double* /*target*/) {}
};

class VectorParameter : public Parameter {
 public:
  explicit VectorParameter(std::vector<std::string>* /*target*/) {}
};

// Stub Option returned by addOption
struct Option {
  std::string name;
};

// Stub Flags enum
enum class Flags : uint16_t { Default = 0 };

constexpr uint16_t makeFlags(Flags /*f*/ = Flags::Default) { return 0; }

class ProgramOptions {
 public:
  ProgramOptions() = default;
  virtual ~ProgramOptions() = default;

  // Real API: Parameter* (takes ownership)
  Option& addOption(std::string const& name, std::string const& /*desc*/,
                    Parameter* param, uint16_t /*flags*/ = 0) {
    _opts.push_back(name);
    delete param;  // mock doesn't need it
    _stubOption.name = name;
    return _stubOption;
  }

  // Real API: unique_ptr<Parameter>
  Option& addOption(std::string const& name, std::string const& /*desc*/,
                    std::unique_ptr<Parameter> /*param*/,
                    uint16_t /*flags*/ = 0) {
    _opts.push_back(name);
    _stubOption.name = name;
    return _stubOption;
  }

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
  Option _stubOption;
};

}  // namespace arangodb::options
