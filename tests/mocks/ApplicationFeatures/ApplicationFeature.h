#pragma once
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::application_features {

class ApplicationFeature {
  friend class ApplicationServer;

 public:
  ApplicationFeature() = delete;
  ApplicationFeature(ApplicationFeature const&) = delete;
  ApplicationFeature& operator=(ApplicationFeature const&) = delete;
  virtual ~ApplicationFeature() = default;

  enum class State {
    UNINITIALIZED,
    INITIALIZED,
    VALIDATED,
    PREPARED,
    STARTED,
    STOPPED,
    UNPREPARED
  };

  ApplicationServer& server() const { return _server; }
  std::string_view name() const noexcept { return _name; }
  size_t registration() const { return _registration; }

  bool isOptional() const { return _optional; }
  bool isRequired() const { return !_optional; }
  State state() const { return _state; }
  bool isEnabled() const { return _enabled; }
  void enable() { _enabled = true; }
  void disable() { _enabled = false; }
  void forceDisable() { _enabled = false; }
  void setEnabled(bool value) { _enabled = value; }

  // Full lifecycle — matches real ArangoDB's 10 virtual methods
  virtual void collectOptions(std::shared_ptr<options::ProgramOptions>) {}
  virtual void loadOptions(std::shared_ptr<options::ProgramOptions>,
                           char const* /*binaryPath*/) {}
  virtual void validateOptions(std::shared_ptr<options::ProgramOptions>) {}
  virtual void daemonize() {}
  virtual void prepare() {}
  virtual void start() {}
  virtual void initiateSoftShutdown() {}
  virtual void beginShutdown() {}
  virtual void stop() {}
  virtual void unprepare() {}

 protected:
  // 3-arg constructor matching real API
  ApplicationFeature(ApplicationServer& server, size_t registration,
                     std::string_view name)
      : _server(server), _registration(registration), _name(name),
        _state(State::UNINITIALIZED), _enabled(true), _optional(false) {}

  // CRTP constructor matching real API (line 169 of real header)
  template<typename Server, typename Impl>
  ApplicationFeature(Server& server, const Impl&)
      : ApplicationFeature(server, Server::template id<Impl>(), Impl::name()) {}

  void setOptional(bool value = true) { _optional = value; }

  // Dependency stubs (no-op in mock, functional in real)
  void dependsOn(size_t /*other*/) {}
  template<typename T, typename Server>
  void startsAfter() {}
  void startsAfter(size_t /*type*/) {}
  template<typename T, typename Server>
  void startsBefore() {}
  void startsBefore(size_t /*type*/) {}
  template<typename T, typename Server>
  void onlyEnabledWith() {}

 private:
  void state(State s) { _state = s; }

  ApplicationServer& _server;
  size_t _registration;
  std::string_view _name;
  State _state;
  bool _enabled;
  bool _optional;
};

// Typed feature template matching real ArangoDB (line 263 of real header)
template<typename ServerT>
class ApplicationFeatureT : public ApplicationFeature {
 public:
  using Server = ServerT;

  Server& server() const noexcept {
    return static_cast<Server&>(ApplicationFeature::server());
  }

  template<typename T>
  void startsAfter() { ApplicationFeature::startsAfter<T, Server>(); }
  template<typename T>
  void startsBefore() { ApplicationFeature::startsBefore<T, Server>(); }
  template<typename T>
  void onlyEnabledWith() { ApplicationFeature::onlyEnabledWith<T, Server>(); }

 protected:
  template<typename Impl>
  ApplicationFeatureT(Server& server, const Impl&)
      : ApplicationFeatureT(server, Server::template id<Impl>(), Impl::name()) {}

  ApplicationFeatureT(Server& server, size_t registration,
                      std::string_view name)
      : ApplicationFeature(server, registration, name) {}
};

}  // namespace arangodb::application_features

namespace arangodb {
// ArangodFeature type alias — matches real ArangoDB's RestServer/arangod.h
using ArangodFeature = application_features::ApplicationFeatureT<ArangodServer>;
}  // namespace arangodb
