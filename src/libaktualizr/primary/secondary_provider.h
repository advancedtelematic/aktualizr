#ifndef UPTANE_SECONDARY_PROVIDER_H
#define UPTANE_SECONDARY_PROVIDER_H

#include <string>

#include "libaktualizr/config.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"

class SecondaryProviderBuilder;

class SecondaryProvider {
 public:
  friend class SecondaryProviderBuilder;

  bool getMetadata(Uptane::MetaBundle* meta_bundle, const Uptane::Target& target) const;
  bool getDirectorMetadata(std::string* root, std::string* targets) const;
  bool getImageRepoMetadata(std::string* root, std::string* timestamp, std::string* snapshot,
                            std::string* targets) const;
  std::string getTreehubCredentials() const;
  std::ifstream getTargetFileHandle(const Uptane::Target& target) const;

 private:
  SecondaryProvider(Config& config_in, const std::shared_ptr<const INvStorage>& storage_in,
                    const std::shared_ptr<const PackageManagerInterface>& package_manager_in)
      : config_(config_in), storage_(storage_in), package_manager_(package_manager_in) {}

  Config& config_;
  const std::shared_ptr<const INvStorage> storage_;
  const std::shared_ptr<const PackageManagerInterface> package_manager_;
};

#endif  // UPTANE_SECONDARY_PROVIDER_H
