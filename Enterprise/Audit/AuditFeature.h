#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Enterprise/Basics/EnterpriseCompat.h"

// Always include our AuditLogger (our own implementation, needed in both modes)
#include "Enterprise/Audit/AuditLogger.h"

// In integration mode, also include real LogTopic for static audit log topics
// that ArangoDB core (LogTopic.cpp) expects as static members.
#if defined(ARANGODB_INTEGRATION_BUILD) || __has_include("Logger/LogTopic.h")
#include "Logger/LogTopic.h"
#endif

namespace arangodb {

namespace options {
class ProgramOptions;
}

class AuditFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Audit"; }

  // Static audit log topics — required by ArangoDB core (LogTopic.cpp).
  // These are defined in AuditFeature.cpp.
#if defined(ARANGODB_INTEGRATION_BUILD) || __has_include("Logger/LogTopic.h")
  static LogTopic AUDIT_AUTHENTICATION;
  static LogTopic AUDIT_AUTHORIZATION;
  static LogTopic AUDIT_DATABASE;
  static LogTopic AUDIT_COLLECTION;
  static LogTopic AUDIT_VIEW;
  static LogTopic AUDIT_DOCUMENT;
  static LogTopic AUDIT_SERVICE;
  static LogTopic AUDIT_HOTBACKUP;
#endif

  explicit AuditFeature(ArangodServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  // Test helper: set output specs without going through option parsing
  void setOutputSpecs(std::vector<std::string> specs);

  // 8 audit topic logging methods
  void logAuthentication(std::string const& username,
                         std::string const& database,
                         std::string const& clientIp,
                         std::string const& authMethod,
                         std::string const& text);

  void logAuthorization(std::string const& username,
                        std::string const& database,
                        std::string const& clientIp,
                        std::string const& authMethod,
                        std::string const& text);

  void logCollection(std::string const& username,
                     std::string const& database,
                     std::string const& clientIp,
                     std::string const& authMethod,
                     std::string const& text);

  void logDatabase(std::string const& username,
                   std::string const& database,
                   std::string const& clientIp,
                   std::string const& authMethod,
                   std::string const& text);

  void logDocument(std::string const& username,
                   std::string const& database,
                   std::string const& clientIp,
                   std::string const& authMethod,
                   std::string const& text);

  void logView(std::string const& username,
               std::string const& database,
               std::string const& clientIp,
               std::string const& authMethod,
               std::string const& text);

  void logService(std::string const& username,
                  std::string const& database,
                  std::string const& clientIp,
                  std::string const& authMethod,
                  std::string const& text);

  void logHotbackup(std::string const& username,
                    std::string const& database,
                    std::string const& clientIp,
                    std::string const& authMethod,
                    std::string const& text);

 private:
  void logEvent(std::string const& topic,
                std::string const& username,
                std::string const& database,
                std::string const& clientIp,
                std::string const& authMethod,
                std::string const& text);

  std::unique_ptr<AuditLogger> _logger;
  std::vector<std::string> _outputSpecs;
};

}  // namespace arangodb
