#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {

/// Minimal VPack-like value for testing AQL functions without full VelocyPack.
/// Production code uses velocypack::Slice; tests use this lightweight mock.
struct MockVPackValue {
  enum Type { Null, Bool, Int, UInt, Double, String, Array };
  Type type = Null;
  bool boolVal = false;
  int64_t intVal = 0;
  uint64_t uintVal = 0;
  double doubleVal = 0.0;
  std::string stringVal;
  std::vector<MockVPackValue> arrayVal;

  bool isArray() const { return type == Array; }
  bool isString() const { return type == String; }
  bool isNumber() const { return type == Int || type == UInt || type == Double; }
  bool isNull() const { return type == Null; }
  bool isBool() const { return type == Bool; }
  bool isNone() const { return false; }  // mock has no None state

  std::string_view stringView() const { return stringVal; }

  std::string toJson() const {
    switch (type) {
      case Null: return "null";
      case Bool: return boolVal ? "true" : "false";
      case Int: return std::to_string(intVal);
      case UInt: return std::to_string(uintVal);
      case Double: return std::to_string(doubleVal);
      case String: return "\"" + stringVal + "\"";
      case Array: {
        std::string result = "[";
        for (size_t i = 0; i < arrayVal.size(); ++i) {
          if (i > 0) result += ",";
          result += arrayVal[i].toJson();
        }
        result += "]";
        return result;
      }
    }
    return "null";
  }

  template <typename T>
  T getNumber() const {
    if (type == Int) return static_cast<T>(intVal);
    if (type == UInt) return static_cast<T>(uintVal);
    return static_cast<T>(doubleVal);
  }

  uint64_t length() const { return arrayVal.size(); }
  MockVPackValue const& at(size_t idx) const { return arrayVal[idx]; }
};

/// Helper to build mock VPack string arrays.
inline MockVPackValue makeStringArray(std::vector<std::string> const& strs) {
  MockVPackValue v;
  v.type = MockVPackValue::Array;
  for (auto const& s : strs) {
    MockVPackValue elem;
    elem.type = MockVPackValue::String;
    elem.stringVal = s;
    v.arrayVal.push_back(elem);
  }
  return v;
}

/// Helper to build mock VPack uint64 arrays.
inline MockVPackValue makeUIntArray(std::vector<uint64_t> const& vals) {
  MockVPackValue v;
  v.type = MockVPackValue::Array;
  for (uint64_t val : vals) {
    MockVPackValue elem;
    elem.type = MockVPackValue::UInt;
    elem.uintVal = val;
    v.arrayVal.push_back(elem);
  }
  return v;
}

/// Helper to build a mock null value.
inline MockVPackValue makeNull() {
  MockVPackValue v;
  v.type = MockVPackValue::Null;
  return v;
}

/// Helper to build a mock integer value.
inline MockVPackValue makeInt(int64_t val) {
  MockVPackValue v;
  v.type = MockVPackValue::Int;
  v.intVal = val;
  return v;
}

/// Helper to build a mock uint value.
inline MockVPackValue makeUInt(uint64_t val) {
  MockVPackValue v;
  v.type = MockVPackValue::UInt;
  v.uintVal = val;
  return v;
}

/// Helper to build a mock double value.
inline MockVPackValue makeDouble(double val) {
  MockVPackValue v;
  v.type = MockVPackValue::Double;
  v.doubleVal = val;
  return v;
}

/// Helper to build a mock string value.
inline MockVPackValue makeString(std::string const& val) {
  MockVPackValue v;
  v.type = MockVPackValue::String;
  v.stringVal = val;
  return v;
}

/// Helper to build a mock bool value.
inline MockVPackValue makeBool(bool val) {
  MockVPackValue v;
  v.type = MockVPackValue::Bool;
  v.boolVal = val;
  return v;
}

}  // namespace arangodb
