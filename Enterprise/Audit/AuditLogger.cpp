#include "AuditLogger.h"

#include <chrono>
#include <iostream>

#ifdef __APPLE__
#include <syslog.h>
#endif

namespace arangodb {

std::string AuditEvent::format() const {
  return timestamp + "|" + server + "|" + topic + "|" + username + "|" +
         database + "|" + clientIp + "|" + authMethod + "|" + text;
}

AuditLogger::~AuditLogger() {
  if (_running.load(std::memory_order_relaxed)) {
    stop();
  }
}

void AuditLogger::addOutput(OutputType type, std::string const& target) {
  switch (type) {
    case OutputType::File: {
      auto stream = std::make_unique<std::ofstream>(target,
          std::ios::out | std::ios::app);
      if (!stream->is_open()) {
        throw std::runtime_error("Cannot open audit file: " + target);
      }
      _fileOutputs.push_back(FileOutput{target, std::move(stream)});
      break;
    }
    case OutputType::Syslog: {
#ifdef __APPLE__
      openlog("arangodb", LOG_PID, LOG_LOCAL0);
#endif
      _syslogOutputs.push_back(SyslogOutput{target});
      break;
    }
  }
}

void AuditLogger::log(AuditEvent event) {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _buffer.push_back(std::move(event));
  }
  _cv.notify_one();
}

void AuditLogger::start() {
  _running.store(true, std::memory_order_release);
  _writerThread = std::thread([this]() { drainLoop(); });
}

void AuditLogger::stop() {
  _running.store(false, std::memory_order_release);
  _cv.notify_one();
  if (_writerThread.joinable()) {
    _writerThread.join();
  }
}

void AuditLogger::drainLoop() {
  while (true) {
    std::deque<AuditEvent> batch;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return !_buffer.empty() || !_running.load(std::memory_order_acquire);
      });
      batch.swap(_buffer);
    }

    for (auto const& event : batch) {
      writeEvent(event);
    }

    // Flush file outputs after each batch
    for (auto& fo : _fileOutputs) {
      if (fo.stream && fo.stream->is_open()) {
        fo.stream->flush();
      }
    }

    // Exit only when stopped AND buffer is drained (Pitfall 5)
    if (!_running.load(std::memory_order_acquire)) {
      std::lock_guard<std::mutex> lock(_mutex);
      if (_buffer.empty()) {
        break;
      }
      // More events arrived between swap and check -- continue draining
    }
  }

  // Final drain: anything left in the buffer after stopping
  std::deque<AuditEvent> remaining;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    remaining.swap(_buffer);
  }
  for (auto const& event : remaining) {
    writeEvent(event);
  }
  for (auto& fo : _fileOutputs) {
    if (fo.stream && fo.stream->is_open()) {
      fo.stream->flush();
    }
  }
}

void AuditLogger::writeEvent(AuditEvent const& event) {
  std::string formatted = event.format();

  for (auto& fo : _fileOutputs) {
    if (fo.stream && fo.stream->is_open()) {
      *fo.stream << formatted << "\n";
    }
  }

#ifdef __APPLE__
  for (auto const& so : _syslogOutputs) {
    (void)so;
    syslog(LOG_INFO, "%s", formatted.c_str());
  }
#endif
}

}  // namespace arangodb
