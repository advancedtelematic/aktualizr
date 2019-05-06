#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "aktualizr_info_config.h"
#include "logging/logging.h"
#include "package_manager/packagemanagerfactory.h"
#include "storage/invstorage.h"
#include "storage/sql_utils.h"

namespace po = boost::program_options;

static void loadAndPrintDelegations(const std::shared_ptr<INvStorage> &storage) {
  std::vector<std::pair<Uptane::Role, std::string> > delegations;
  bool delegations_fetch_res = storage->loadAllDelegations(delegations);

  if (!delegations_fetch_res) {
    std::cout << "Failed to load delegations" << std::endl;
    return;
  }

  if (delegations.size() == 0) {
    std::cout << "Delegations are not present" << std::endl;
  }

  std::cout << "Delegations:" << std::endl;
  for (const auto &delegation : delegations) {
    std::cout << delegation.first << ": " << delegation.second << std::endl;
  }
}

int main(int argc, char **argv) {
  po::options_description desc("aktualizr-info command line options");
  // clang-format off
  desc.add_options()
    ("help,h", "print usage")
    ("config,c", po::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
    ("loglevel", po::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
    ("name-only",  "Only output device name (intended for scripting)")
    ("tls-creds",  "Outputs TLS credentials")
    ("ecu-keys",  "Outputs UPTANE keys")
    ("images-root",  "Outputs root.json from images repo")
    ("images-target",  "Outputs targets.json from images repo")
    ("delegation",  "Outputs metadata of image repo targets' delegations")
    ("director-root",  "Outputs root.json from director repo")
    ("director-target",  "Outputs targets.json from director repo")
    ("allow-migrate", "Opens database in read/write mode to make possible to migrate database if needed");
  // clang-format on

  try {
    logger_init();
    logger_set_threshold(boost::log::trivial::info);
    po::variables_map vm;
    po::basic_parsed_options<char> parsed_options = po::command_line_parser(argc, argv).options(desc).run();
    po::store(parsed_options, vm);
    po::notify(vm);
    if (vm.count("help") != 0) {
      std::cout << desc << '\n';
      exit(EXIT_SUCCESS);
    }

    AktualizrInfoConfig config(vm);

    bool readonly = true;
    if (vm.count("allow-migrate") != 0u) {
      readonly = false;
    }

    std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage, readonly);

    std::string director_root;
    std::string director_targets;
    std::string images_root;
    std::string images_targets;

    bool has_metadata = storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director());
    storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Image());
    storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
    storage->loadNonRoot(&images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

    std::string device_id;
    if (!storage->loadDeviceId(&device_id)) {
      std::cout << "Couldn't load device ID" << std::endl;
    } else {
      // Early return if only printing device ID.
      if (vm.count("name-only") != 0u) {
        std::cout << device_id << std::endl;
        return 0;
      }
      std::cout << "Device ID: " << device_id << std::endl;
    }

    EcuSerials serials;
    if (!storage->loadEcuSerials(&serials)) {
      std::cout << "Couldn't load ECU serials" << std::endl;
    } else if (serials.size() == 0) {
      std::cout << "Primary serial is not found" << std::endl;
    } else {
      std::cout << "Primary ecu serial ID: " << serials[0].first << std::endl;
      std::cout << "Primary ecu hardware ID: " << serials[0].second << std::endl;
    }
    if (serials.size() > 1) {
      auto it = serials.begin() + 1;
      std::cout << "Secondaries:\n";
      int secondary_number = 1;
      for (; it != serials.end(); ++it) {
        std::cout << secondary_number++ << ") serial ID: " << it->first << std::endl;
        std::cout << "   hardware ID: " << it->second << std::endl;

        std::vector<Uptane::Target> installed_targets;
        size_t current_version = SIZE_MAX;
        size_t pending_version = SIZE_MAX;

        auto load_installed_version_res = storage->loadInstalledVersions((it->first).ToString(), &installed_targets,
                                                                         &current_version, &pending_version);

        if (!load_installed_version_res ||
            (current_version >= installed_targets.size() && pending_version >= installed_targets.size())) {
          std::cout << "   no details about installed nor pending images\n";
        } else {
          if (installed_targets.size() > current_version) {
            std::cout << "   installed image hash: " << installed_targets[current_version].sha256Hash() << "\n";
            std::cout << "   installed image filename: " << installed_targets[current_version].filename() << "\n";
          }
          if (installed_targets.size() > pending_version) {
            std::cout << "   pending image hash: " << installed_targets[pending_version].sha256Hash() << "\n";
            std::cout << "   pending image filename: " << installed_targets[pending_version].filename() << "\n";
          }
        }
      }
    }

    std::vector<MisconfiguredEcu> misconfigured_ecus;
    storage->loadMisconfiguredEcus(&misconfigured_ecus);
    if (misconfigured_ecus.size() != 0u) {
      std::cout << "Removed or not registered ecus:" << std::endl;
      std::vector<MisconfiguredEcu>::const_iterator it;
      for (it = misconfigured_ecus.begin(); it != misconfigured_ecus.end(); ++it) {
        std::cout << "   '" << it->serial << "' with hardware_id '" << it->hardware_id << "' "
                  << (it->state == EcuState::kOld ? "has been removed from config" : "not registered yet") << std::endl;
      }
    }

    auto registered = storage->loadEcuRegistered();
    std::cout << "Provisioned on server: " << (registered ? "yes" : "no") << std::endl;
    std::cout << "Fetched metadata: " << (has_metadata ? "yes" : "no") << std::endl;

    if (has_metadata) {
      if (vm.count("images-root") != 0u) {
        std::cout << "image root.json content:" << std::endl;
        std::cout << images_root << std::endl;
      }

      if (vm.count("images-target") != 0u) {
        std::cout << "image targets.json content:" << std::endl;
        std::cout << images_targets << std::endl;
      }

      if (vm.count("delegation") != 0u) {
        loadAndPrintDelegations(storage);
      }

      if (vm.count("director-root") != 0u) {
        std::cout << "director root.json content:" << std::endl;
        std::cout << director_root << std::endl;
      }

      if (vm.count("director-target") != 0u) {
        std::cout << "director targets.json content:" << std::endl;
        std::cout << director_targets << std::endl;
      }
    }

    if (vm.count("tls-creds") != 0u) {
      std::string ca;
      std::string cert;
      std::string pkey;

      storage->loadTlsCreds(&ca, &cert, &pkey);
      std::cout << "Root CA certificate:" << std::endl << ca << std::endl;
      std::cout << "Client certificate:" << std::endl << cert << std::endl;
      std::cout << "Client private key:" << std::endl << pkey << std::endl;
    }

    if (vm.count("ecu-keys") != 0u) {
      std::string priv;
      std::string pub;

      storage->loadPrimaryKeys(&pub, &priv);
      std::cout << "Public key:" << std::endl << pub << std::endl;
      std::cout << "Private key:" << std::endl << priv << std::endl;
    }

    auto pacman = PackageManagerFactory::makePackageManager(config.pacman, storage, nullptr, nullptr);

    Uptane::Target current_target = pacman->getCurrent();

    if (current_target.IsValid()) {
      std::cout << "Current primary ecu running version: " << current_target.sha256Hash() << std::endl;
    } else {
      std::cout << "No currently running version on primary ecu" << std::endl;
    }

    std::vector<Uptane::Target> installed_versions;
    size_t pending = SIZE_MAX;
    storage->loadInstalledVersions("", &installed_versions, nullptr, &pending);

    if (pending != SIZE_MAX) {
      std::cout << "Pending primary ecu version: " << installed_versions[pending].sha256Hash() << std::endl;
    }

  } catch (const po::error &o) {
    std::cout << o.what() << std::endl;
    std::cout << desc;
    return EXIT_FAILURE;

  } catch (const std::exception &exc) {
    std::cerr << "Error: " << exc.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
