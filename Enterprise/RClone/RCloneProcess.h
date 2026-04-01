#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Enterprise/RClone/RCloneConfig.h"

namespace arangodb {

/// Result of an rclone subprocess execution.
struct RCloneResult {
  int exitCode;
  std::string stdoutOutput;
  std::string stderrOutput;
  bool timedOut;
};

/// Progress callback: receives percentage (0-100).
using RCloneProgressCallback = std::function<void(uint32_t percent)>;

class RCloneProcess {
 public:
  /// Build the rclone command-line arguments for a copy operation.
  /// Does NOT include credentials -- those go in the environment.
  static std::vector<std::string> buildCommand(
      RCloneConfig const& config,
      std::string const& localPath);

  /// Build the environment variable map for credential injection.
  /// All credentials are injected here, never as CLI args.
  static std::unordered_map<std::string, std::string> buildEnvironment(
      RCloneConfig const& config);

  /// Parse rclone progress output line and extract percentage.
  static std::optional<uint32_t> parseProgressLine(
      std::string_view line);

  /// Execute rclone subprocess synchronously.
  /// Calls progressCallback on each progress line if provided.
  /// Returns result with exit code, captured output, and timeout flag.
  static RCloneResult execute(
      RCloneConfig const& config,
      std::string const& localPath,
      RCloneProgressCallback progressCallback = nullptr);

 private:
  /// Fork and exec rclone with the given args and environment.
  /// Monitors stdout for progress lines.
  /// Enforces config.timeoutSeconds as IO idle timeout.
  static RCloneResult forkAndExec(
      std::vector<std::string> const& args,
      std::unordered_map<std::string, std::string> const& env,
      uint64_t timeoutSeconds,
      RCloneProgressCallback const& progressCallback);
};

}  // namespace arangodb
