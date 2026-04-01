#pragma once
/// Mock ProgramOptions for standalone builds.
/// Matches real ArangoDB ProgramOptions/Parameters.h API surface.
/// In integration mode, the real headers are used instead of this mock.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::options {

// ---------------------------------------------------------------------------
// Mock Parameter hierarchy (matches real ArangoDB ProgramOptions/Parameters.h)
// Real ArangoDB uses structs, not classes.
// ---------------------------------------------------------------------------
struct Parameter {
  virtual ~Parameter() = default;
  virtual void flushValue() {}
  virtual bool requiresValue() const { return true; }
  virtual std::string name() const { return ""; }
  virtual std::string valueString() const { return ""; }
  virtual std::string set(std::string const&) { return ""; }
};

struct StringParameter : public Parameter {
  using ValueType = std::string;
  explicit StringParameter(std::string* /*target*/) {}
  std::string name() const override { return "string"; }
};

// BooleanParameter — real API is BooleanParameterBase<bool>
struct BooleanParameter : public Parameter {
  using ValueType = bool;
  explicit BooleanParameter(bool* /*target*/) {}
  bool requiresValue() const override { return false; }
  std::string name() const override { return "boolean"; }
};

// NumericParameter<T> — template matching real API
template<typename T>
struct NumericParameter : public Parameter {
  using ValueType = T;
  explicit NumericParameter(T* /*target*/) {}
  NumericParameter(T* /*target*/, T /*min*/, T /*max*/) {}
  std::string name() const override { return "numeric"; }
};

using Int16Parameter = NumericParameter<std::int16_t>;
using UInt16Parameter = NumericParameter<std::uint16_t>;
using Int32Parameter = NumericParameter<std::int32_t>;
using UInt32Parameter = NumericParameter<std::uint32_t>;
using Int64Parameter = NumericParameter<std::int64_t>;
using UInt64Parameter = NumericParameter<std::uint64_t>;
using SizeTParameter = NumericParameter<std::size_t>;
using DoubleParameter = NumericParameter<double>;

// VectorParameter<T> — real API is template taking vector of T::ValueType
template<typename T>
struct VectorParameter : public Parameter {
  using ValueType = typename T::ValueType;
  explicit VectorParameter(std::vector<ValueType>* /*target*/) {}
  std::string name() const override { return "vector"; }
};

// DiscreteValuesParameter<T> — restricts to allowed set
template<typename T>
struct DiscreteValuesParameter : public T {
  using ValueType = typename T::ValueType;
  template<typename... Args>
  DiscreteValuesParameter(Args&&... args) : T(std::forward<Args>(args)...) {}
};

// DiscreteValuesVectorParameter<T>
template<typename T>
struct DiscreteValuesVectorParameter : public VectorParameter<T> {
  using ValueType = typename T::ValueType;
  template<typename... Args>
  DiscreteValuesVectorParameter(Args&&... args)
      : VectorParameter<T>(std::forward<Args>(args)...) {}
};

// ObsoleteParameter — marks removed options
struct ObsoleteParameter : public Parameter {
  bool requiresValue() const override { return false; }
  std::string name() const override { return "obsolete"; }
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
