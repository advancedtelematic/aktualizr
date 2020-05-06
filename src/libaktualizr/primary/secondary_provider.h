#ifndef UPTANE_SECONDARY_PROVIDER_H
#define UPTANE_SECONDARY_PROVIDER_H

#include <string>

#include "config/config.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"

class SecondaryProvider {
 public:
  SecondaryProvider(Config& config_in, const std::shared_ptr<const INvStorage>& storage_in)
      : config_(config_in), storage_(storage_in) {}

  bool getMetadata(Uptane::MetaBundle* meta_bundle, const Uptane::Target& target) const;
  bool getDirectorMetadata(std::string* root, std::string* targets) const;
  bool getImageRepoMetadata(std::string* root, std::string* timestamp, std::string* snapshot,
                            std::string* targets) const;
  std::string getTreehubCredentials() const;
  std::unique_ptr<StorageTargetRHandle> getTargetFileHandle(const Uptane::Target& target) const;

 private:
  Config& config_;
  const std::shared_ptr<const INvStorage> storage_;
};

#endif  // UPTANE_SECONDARY_PROVIDER_H
