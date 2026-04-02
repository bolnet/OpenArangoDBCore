#include "Enterprise/RClone/RCloneProcess.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <sstream>

#ifdef __APPLE__
#include <crt_externs.h>
#define ENVIRON (*_NSGetEnviron())
#else
extern "C" { extern char** environ; }
#define ENVIRON environ
#endif

#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace arangodb {

std::vector<std::string> RCloneProcess::buildCommand(
    RCloneConfig const& config,
    std::string const& localPath) {
  std::string remoteDest = "remote:" + config.bucket;
  if (!config.pathPrefix.empty()) {
    remoteDest += "/" + config.pathPrefix;
  }

  return {
    config.rcloneBinaryPath,
    "copy",
    localPath,
    remoteDest,
    "--progress",
    "--stats", "1s",
    "--transfers", std::to_string(config.transfers),
    "--retries", std::to_string(config.retries),
    "--timeout", std::to_string(config.timeoutSeconds) + "s",
    "--log-level", "INFO"
  };
}

std::unordered_map<std::string, std::string> RCloneProcess::buildEnvironment(
    RCloneConfig const& config) {
  std::unordered_map<std::string, std::string> env;

  env["RCLONE_CONFIG_REMOTE_TYPE"] =
      std::string(cloudProviderToRCloneType(config.provider));

  switch (config.provider) {
    case CloudProvider::kS3:
      env["RCLONE_CONFIG_REMOTE_ACCESS_KEY_ID"] = config.accessKeyId;
      env["RCLONE_CONFIG_REMOTE_SECRET_ACCESS_KEY"] = config.secretAccessKey;
      if (!config.endpoint.empty()) {
        env["RCLONE_CONFIG_REMOTE_ENDPOINT"] = config.endpoint;
      }
      if (!config.region.empty()) {
        env["RCLONE_CONFIG_REMOTE_REGION"] = config.region;
      }
      break;

    case CloudProvider::kAzureBlob:
      env["RCLONE_CONFIG_REMOTE_ACCOUNT"] = config.azureAccount;
      env["RCLONE_CONFIG_REMOTE_KEY"] = config.azureKey;
      break;

    case CloudProvider::kGCS:
      env["RCLONE_CONFIG_REMOTE_SERVICE_ACCOUNT_FILE"] =
          config.gcsServiceAccountFile;
      env["RCLONE_CONFIG_REMOTE_BUCKET_POLICY_ONLY"] = "true";
      break;
  }

  return env;
}

std::optional<uint32_t> RCloneProcess::parseProgressLine(
    std::string_view line) {
  // Match pattern: "Transferred: ... / ..., <N>%, ..."
  // We look for a percentage in the first Transferred line (bytes).
  static std::regex const kProgressRegex(
      R"(Transferred:\s+[\d.]+ \S+ / [\d.]+ \S+, (\d+)%)");

  std::string lineStr(line);
  std::smatch match;
  if (std::regex_search(lineStr, match, kProgressRegex)) {
    try {
      auto value = static_cast<uint32_t>(std::stoul(match[1].str()));
      if (value <= 100) {
        return value;
      }
    } catch (...) {
      // parse failure
    }
  }
  return std::nullopt;
}

RCloneResult RCloneProcess::execute(
    RCloneConfig const& config,
    std::string const& localPath,
    RCloneProgressCallback progressCallback) {
  auto args = buildCommand(config, localPath);
  auto env = buildEnvironment(config);
  return forkAndExec(args, env, config.timeoutSeconds, progressCallback);
}

RCloneResult RCloneProcess::forkAndExec(
    std::vector<std::string> const& args,
    std::unordered_map<std::string, std::string> const& env,
    uint64_t timeoutSeconds,
    RCloneProgressCallback const& progressCallback) {

  if (args.empty()) {
    return RCloneResult{-1, "", "empty command", false};
  }

  // Create pipes for stdout and stderr capture.
  int stdoutPipe[2];
  int stderrPipe[2];
  if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
    return RCloneResult{-1, "", "pipe() failed: " +
        std::string(strerror(errno)), false};
  }

  // Build environment: inherit current env plus overlay our vars.
  std::vector<std::string> envStrings;
  for (char** e = ENVIRON; *e != nullptr; ++e) {
    envStrings.emplace_back(*e);
  }
  for (auto const& [key, val] : env) {
    envStrings.push_back(key + "=" + val);
  }
  std::vector<char*> envPtrs;
  envPtrs.reserve(envStrings.size() + 1);
  for (auto& s : envStrings) {
    envPtrs.push_back(s.data());
  }
  envPtrs.push_back(nullptr);

  // Build argv.
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (auto const& a : args) {
    argv.push_back(const_cast<char*>(a.c_str()));
  }
  argv.push_back(nullptr);

  // Set up posix_spawn file actions for pipe redirection.
  posix_spawn_file_actions_t fileActions;
  posix_spawn_file_actions_init(&fileActions);
  posix_spawn_file_actions_addclose(&fileActions, stdoutPipe[0]);
  posix_spawn_file_actions_addclose(&fileActions, stderrPipe[0]);
  posix_spawn_file_actions_adddup2(&fileActions, stdoutPipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&fileActions, stderrPipe[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&fileActions, stdoutPipe[1]);
  posix_spawn_file_actions_addclose(&fileActions, stderrPipe[1]);

  pid_t pid = -1;
  int spawnResult = posix_spawn(&pid, argv[0], &fileActions, nullptr,
                                argv.data(), envPtrs.data());
  posix_spawn_file_actions_destroy(&fileActions);

  // Close write ends in parent.
  close(stdoutPipe[1]);
  close(stderrPipe[1]);

  if (spawnResult != 0) {
    close(stdoutPipe[0]);
    close(stderrPipe[0]);
    return RCloneResult{-1, "",
        "posix_spawn failed: " + std::string(strerror(spawnResult)), false};
  }

  // Read stdout and stderr using poll() with timeout.
  std::string stdoutBuf;
  std::string stderrBuf;
  bool timedOut = false;

  struct pollfd fds[2];
  fds[0].fd = stdoutPipe[0];
  fds[0].events = POLLIN;
  fds[1].fd = stderrPipe[0];
  fds[1].events = POLLIN;

  int openFds = 2;
  int timeoutMs = static_cast<int>(timeoutSeconds) * 1000;
  if (timeoutMs <= 0) {
    timeoutMs = 300000;  // 5 minute default
  }

  std::string lineBuffer;

  while (openFds > 0) {
    int ret = poll(fds, 2, timeoutMs);
    if (ret == 0) {
      // Timeout -- kill the process.
      timedOut = true;
      kill(pid, SIGTERM);
      // Give 5 seconds to terminate gracefully.
      usleep(5000000);
      int status = 0;
      pid_t waited = waitpid(pid, &status, WNOHANG);
      if (waited == 0) {
        kill(pid, SIGKILL);
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
      }
      break;
    }
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    // Read stdout.
    if (fds[0].revents & (POLLIN | POLLHUP)) {
      std::array<char, 4096> buf{};
      ssize_t n = read(stdoutPipe[0], buf.data(), buf.size());
      if (n > 0) {
        stdoutBuf.append(buf.data(), static_cast<size_t>(n));

        // Parse progress lines from the buffer.
        if (progressCallback) {
          lineBuffer.append(buf.data(), static_cast<size_t>(n));
          std::string::size_type pos;
          while ((pos = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            lineBuffer.erase(0, pos + 1);
            auto percent = parseProgressLine(line);
            if (percent.has_value()) {
              progressCallback(percent.value());
            }
          }
          // Also check for \r (rclone uses \r for progress updates).
          while ((pos = lineBuffer.find('\r')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            lineBuffer.erase(0, pos + 1);
            auto percent = parseProgressLine(line);
            if (percent.has_value()) {
              progressCallback(percent.value());
            }
          }
        }
      } else if (n == 0) {
        fds[0].fd = -1;
        --openFds;
      }
    }

    // Read stderr.
    if (fds[1].revents & (POLLIN | POLLHUP)) {
      std::array<char, 4096> buf{};
      ssize_t n = read(stderrPipe[0], buf.data(), buf.size());
      if (n > 0) {
        stderrBuf.append(buf.data(), static_cast<size_t>(n));
      } else if (n == 0) {
        fds[1].fd = -1;
        --openFds;
      }
    }
  }

  close(stdoutPipe[0]);
  close(stderrPipe[0]);

  // Reap the child if not already reaped.
  if (!timedOut) {
    int status = 0;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return RCloneResult{exitCode, std::move(stdoutBuf),
                        std::move(stderrBuf), false};
  }

  return RCloneResult{-1, std::move(stdoutBuf), std::move(stderrBuf), true};
}

}  // namespace arangodb
