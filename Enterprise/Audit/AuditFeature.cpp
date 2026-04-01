#include "AuditFeature.h"

#include "ProgramOptions/ProgramOptions.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#ifdef __has_include
#if __has_include(<unistd.h>)
#include <unistd.h>
#endif
#endif

#include "ProgramOptions/ProgramOptions.h"

static_assert(!std::is_abstract_v<arangodb::AuditFeature>,
              "AuditFeature must not be abstract");

namespace arangodb {

namespace {

std::string currentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

std::string hostname() {
  char buf[256] = {};
  if (gethostname(buf, sizeof(buf)) == 0) {
    return std::string(buf);
  }
  return "unknown";
}

}  // namespace

AuditFeature::AuditFeature(ArangodServer& server)
    : ArangodFeature(server, *this) {}

void AuditFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--audit.output",
                     "audit log output destination(s), e.g. "
                     "file:///path/to/audit.log or syslog://local0",
                     new options::VectorParameter<options::StringParameter>(&_outputSpecs));
}

void AuditFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  for (auto const& spec : _outputSpecs) {
    if (spec.substr(0, 7) != "file://" &&
        spec.substr(0, 9) != "syslog://") {
      throw std::runtime_error(
          "Invalid --audit.output spec: '" + spec +
          "'. Expected file:///path or syslog://facility");
    }
  }
}

void AuditFeature::prepare() {
  _logger = std::make_unique<AuditLogger>();

  for (auto const& spec : _outputSpecs) {
    if (spec.substr(0, 7) == "file://") {
      std::string path = spec.substr(7);
      _logger->addOutput(AuditLogger::OutputType::File, path);
    } else if (spec.substr(0, 9) == "syslog://") {
      std::string facility = spec.substr(9);
      _logger->addOutput(AuditLogger::OutputType::Syslog, facility);
    }
  }
}

void AuditFeature::start() {
  if (_logger) {
    _logger->start();
  }
}

void AuditFeature::beginShutdown() {
  // Nothing specific needed -- stop() handles flush
}

void AuditFeature::stop() {
  if (_logger) {
    _logger->stop();
  }
}

void AuditFeature::unprepare() {
  _logger.reset();
}

void AuditFeature::setOutputSpecs(std::vector<std::string> specs) {
  _outputSpecs = std::move(specs);
}

void AuditFeature::logEvent(std::string const& topic,
                            std::string const& username,
                            std::string const& database,
                            std::string const& clientIp,
                            std::string const& authMethod,
                            std::string const& text) {
  if (!_logger) {
    return;
  }
  AuditEvent event{
    .timestamp = currentTimestamp(),
    .server = hostname(),
    .topic = topic,
    .username = username,
    .database = database,
    .clientIp = clientIp,
    .authMethod = authMethod,
    .text = text
  };
  _logger->log(std::move(event));
}

void AuditFeature::logAuthentication(std::string const& username,
                                     std::string const& database,
                                     std::string const& clientIp,
                                     std::string const& authMethod,
                                     std::string const& text) {
  logEvent("authentication", username, database, clientIp, authMethod, text);
}

void AuditFeature::logAuthorization(std::string const& username,
                                    std::string const& database,
                                    std::string const& clientIp,
                                    std::string const& authMethod,
                                    std::string const& text) {
  logEvent("authorization", username, database, clientIp, authMethod, text);
}

void AuditFeature::logCollection(std::string const& username,
                                 std::string const& database,
                                 std::string const& clientIp,
                                 std::string const& authMethod,
                                 std::string const& text) {
  logEvent("collection", username, database, clientIp, authMethod, text);
}

void AuditFeature::logDatabase(std::string const& username,
                               std::string const& database,
                               std::string const& clientIp,
                               std::string const& authMethod,
                               std::string const& text) {
  logEvent("database", username, database, clientIp, authMethod, text);
}

void AuditFeature::logDocument(std::string const& username,
                               std::string const& database,
                               std::string const& clientIp,
                               std::string const& authMethod,
                               std::string const& text) {
  logEvent("document", username, database, clientIp, authMethod, text);
}

void AuditFeature::logView(std::string const& username,
                           std::string const& database,
                           std::string const& clientIp,
                           std::string const& authMethod,
                           std::string const& text) {
  logEvent("view", username, database, clientIp, authMethod, text);
}

void AuditFeature::logService(std::string const& username,
                              std::string const& database,
                              std::string const& clientIp,
                              std::string const& authMethod,
                              std::string const& text) {
  logEvent("service", username, database, clientIp, authMethod, text);
}

void AuditFeature::logHotbackup(std::string const& username,
                                std::string const& database,
                                std::string const& clientIp,
                                std::string const& authMethod,
                                std::string const& text) {
  logEvent("hotbackup", username, database, clientIp, authMethod, text);
}

}  // namespace arangodb
