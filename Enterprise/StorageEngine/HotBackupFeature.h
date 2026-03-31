#pragma once
#ifndef ARANGODB_HOT_BACKUP_FEATURE_H
#define ARANGODB_HOT_BACKUP_FEATURE_H

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class HotBackupFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "HotBackup"; }
  explicit HotBackupFeature(application_features::ApplicationServer& server)
      : ApplicationFeature(server, *this) {}
};

}  // namespace arangodb

#endif  // ARANGODB_HOT_BACKUP_FEATURE_H
