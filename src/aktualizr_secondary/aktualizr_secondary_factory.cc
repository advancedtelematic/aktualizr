#include "aktualizr_secondary_factory.h"

#include "crypto/keymanager.h"

#include "package_manager/packagemanagerfactory.h"

// TODO: consider implementation of a proper registry/builder/factory
AktualizrSecondary::Ptr AktualizrSecondaryFactory::create(const AktualizrSecondaryConfig& config) {
  auto storage = INvStorage::newStorage(config.storage);
  return AktualizrSecondaryFactory::create(config, storage);
}

AktualizrSecondary::Ptr AktualizrSecondaryFactory::create(const AktualizrSecondaryConfig& config,
                                                          const std::shared_ptr<INvStorage>& storage) {

  auto key_mngr = std::make_shared<KeyManager>(storage, config.keymanagerConfig());
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);

  return std::make_shared<AktualizrSecondary>(config, storage, key_mngr, pacman);
}
