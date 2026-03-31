#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arangodb {

struct AuditEvent {
  std::string timestamp;
  std::string server;
  std::string topic;
  std::string username;
  std::string database;
  std::string clientIp;
  std::string authMethod;
  std::string text;

  std::string format() const;
};

class AuditLogger {
 public:
  enum class OutputType { File, Syslog };

  AuditLogger() = default;
  ~AuditLogger();

  AuditLogger(AuditLogger const&) = delete;
  AuditLogger& operator=(AuditLogger const&) = delete;

  void addOutput(OutputType type, std::string const& target);
  void log(AuditEvent event);
  void start();
  void stop();

 private:
  void drainLoop();
  void writeEvent(AuditEvent const& event);

  std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<AuditEvent> _buffer;
  std::atomic<bool> _running{false};
  std::thread _writerThread;

  struct FileOutput {
    std::string path;
    std::unique_ptr<std::ofstream> stream;
  };
  std::vector<FileOutput> _fileOutputs;

  struct SyslogOutput {
    std::string facility;
  };
  std::vector<SyslogOutput> _syslogOutputs;
};

}  // namespace arangodb
