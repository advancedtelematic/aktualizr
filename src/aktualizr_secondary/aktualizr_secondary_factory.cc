#include "aktualizr_secondary_factory.h"

#include "crypto/keymanager.h"
#include "update_agent_file.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"
#include "update_agent_ostree.h"
#endif

const std::string AktualizrSecondaryFactory::BinaryUpdateDefaultFile{"firmware.txt"};

AktualizrSecondary::Ptr AktualizrSecondaryFactory::create(const AktualizrSecondaryConfig& config) {
  auto storage = INvStorage::newStorage(config.storage);
  return AktualizrSecondaryFactory::create(config, storage);
}

AktualizrSecondary::Ptr AktualizrSecondaryFactory::create(const AktualizrSecondaryConfig& config,
                                                          const std::shared_ptr<INvStorage>& storage) {
  auto key_mngr = std::make_shared<KeyManager>(storage, config.keymanagerConfig());

  UpdateAgent::Ptr update_agent;

  if (config.pacman.type != PACKAGE_MANAGER_OSTREE) {
    std::string current_target_name;

    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;
    auto installed_version_res = storage->loadInstalledVersions("", &current_version, &pending_version);

    if (installed_version_res && !!current_version) {
      current_target_name = current_version->filename();
    } else {
      current_target_name = "unknown";
    }

    update_agent =
        std::make_shared<FileUpdateAgent>(config.storage.path / BinaryUpdateDefaultFile, current_target_name);
  }
#ifdef BUILD_OSTREE
  else {
    std::shared_ptr<OstreeManager> pack_man =
        std::make_shared<OstreeManager>(config.pacman, config.bootloader, storage, nullptr);
    update_agent =
        std::make_shared<OstreeUpdateAgent>(config.pacman.sysroot, key_mngr, pack_man, config.uptane.ecu_hardware_id);
  }
#endif

  return std::make_shared<AktualizrSecondary>(config, storage, key_mngr, update_agent);
}
