#include "Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace arangodb {
namespace enterprise {

std::optional<EncryptionSecret> loadKeyFromFile(std::string const& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  // Read entire file content
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

  // AES-256 requires exactly 32 bytes
  if (content.size() != 32) {
    return std::nullopt;
  }

  EncryptionSecret secret;
  secret.key = std::move(content);

  // Use filename as key ID
  std::filesystem::path p(path);
  secret.id = p.filename().string();

  return secret;
}

std::vector<EncryptionSecret> loadKeysFromFolder(std::string const& path) {
  std::vector<EncryptionSecret> keys;

  std::error_code ec;
  if (!std::filesystem::is_directory(path, ec)) {
    return keys;
  }

  for (auto const& entry : std::filesystem::directory_iterator(path, ec)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto secret = loadKeyFromFile(entry.path().string());
    if (secret.has_value()) {
      keys.push_back(std::move(*secret));
    }
  }

  // Sort by key ID for deterministic ordering
  std::sort(keys.begin(), keys.end(),
            [](EncryptionSecret const& a, EncryptionSecret const& b) {
              return a.id < b.id;
            });

  return keys;
}

}  // namespace enterprise
}  // namespace arangodb
