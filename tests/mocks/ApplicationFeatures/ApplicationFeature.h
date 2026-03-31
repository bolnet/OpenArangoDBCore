#pragma once
#include <memory>
#include <string_view>
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::application_features {

class ApplicationFeature {
 public:
  template<typename Impl>
  ApplicationFeature(ApplicationServer& server, const Impl&)
      : _server(server) {}
  virtual ~ApplicationFeature() = default;

  virtual void collectOptions(std::shared_ptr<options::ProgramOptions>) {}
  virtual void validateOptions(std::shared_ptr<options::ProgramOptions>) {}
  virtual void prepare() {}
  virtual void start() {}
  virtual void beginShutdown() {}
  virtual void stop() {}
  virtual void unprepare() {}

 protected:
  ApplicationServer& _server;
};

}  // namespace arangodb::application_features
