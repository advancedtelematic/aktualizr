#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "aktualizr_info_config.h"
#include "logging/logging.h"
#include "package_manager/packagemanagerfactory.h"
#include "storage/invstorage.h"
#include "storage/sql_utils.h"
#include "utilities/aktualizr_version.h"

namespace bpo = boost::program_options;

static int loadAndPrintDelegations(const std::shared_ptr<INvStorage> &storage) {
  std::vector<std::pair<Uptane::Role, std::string> > delegations;
  bool delegations_fetch_res = storage->loadAllDelegations(delegations);

  if (!delegations_fetch_res) {
    std::cout << "Failed to load delegations" << std::endl;
    return EXIT_FAILURE;
  }

  if (delegations.size() > 0) {
    for (const auto &delegation : delegations) {
      std::cout << delegation.first << ": " << delegation.second << std::endl;
    }
  } else {
    std::cout << "Delegations are not present" << std::endl;
  }
  return EXIT_SUCCESS;
}

void checkInfoOptions(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-info version is: " << aktualizr_version() << "\n";
    exit(EXIT_SUCCESS);
  }
}

int main(int argc, char **argv) {
  bpo::options_description all("aktualizr-info command line options");
  bpo::options_description description("aktualizr-info command line options");
  bpo::options_description hidden("deprecated options");
  // clang-format off
  description.add_options()
    ("help,h", "print usage")
    ("version,v", "Current aktualizr version")
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
    ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
    ("name-only",  "Only output device name (intended for scripting). Cannot be used in combination with other arguments.")
    ("tls-creds",  "Outputs TLS credentials")
    ("tls-root-ca", "Outputs TLS Root CA")
    ("tls-cert", "Outputs TLS client certificate")
    ("tls-prv-key", "Output TLS client private key")
    ("ecu-keys",  "Outputs Uptane keys")
    ("ecu-pub-key", "Outputs Uptane public key")
    ("ecu-prv-key", "Outputs Uptane private key")
    ("image-root",  "Outputs root.json from Image repo")
    ("image-timestamp", "Outputs timestamp.json from Image repo")
    ("image-snapshot", "Outputs snapshot.json from Image repo")
    ("image-targets",  "Outputs targets.json from Image repo")
    ("delegation",  "Outputs metadata of Image repo Targets' delegations")
    ("director-root",  "Outputs root.json from Director repo")
    ("director-targets",  "Outputs targets.json from Director repo")
    ("allow-migrate", "Opens database in read/write mode to make possible to migrate database if needed")
    ("wait-until-provisioned", "Outputs metadata when device already provisioned");
  // Support old names and variations due to common typos.
  hidden.add_options()
    ("images-root",  "Outputs root.json from Image repo")
    ("images-timestamp", "Outputs timestamp.json from Image repo")
    ("images-snapshot", "Outputs snapshot.json from Image repo")
    ("images-target",  "Outputs targets.json from Image repo")
    ("images-targets",  "Outputs targets.json from Image repo")
    ("image-target",  "Outputs targets.json from Image repo")
    ("director-target",  "Outputs targets.json from Director repo");
  // clang-format on

  try {
    all.add(description).add(hidden);
    bpo::variables_map vm;
    bpo::basic_parsed_options<char> parsed_options = bpo::command_line_parser(argc, argv).options(all).run();
    bpo::store(parsed_options, vm);
    checkInfoOptions(description, vm);
    bpo::notify(vm);
    std::vector<std::string> unregistered_options =
        bpo::collect_unrecognized(parsed_options.options, bpo::include_positional);
    if (vm.count("help") == 0 && !unregistered_options.empty()) {
      std::cout << description << "\n";
      exit(EXIT_FAILURE);
    }

    logger_init();
    if (vm.count("loglevel") == 0U) {
      logger_set_enable(false);
    }

    AktualizrInfoConfig config(vm);

    bool secondary_db = false;

    bool readonly = true;
    if (vm.count("allow-migrate") != 0U) {
      readonly = false;
    }

    bool wait_provisioning = false;
    if (vm.count("wait-until-provisioned") != 0) {
      wait_provisioning = true;
    }

    std::shared_ptr<INvStorage> storage;
    bool cmd_trigger = false;
    std::string device_id;

    bool registered = false;
    bool has_metadata = false;
    std::string director_root;
    if (wait_provisioning) {
      while (!registered || !has_metadata) {
        try {
          storage = INvStorage::newStorage(config.storage, readonly);

          registered = storage->loadEcuRegistered();
          has_metadata = storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director());
        } catch (std::exception &e) {
          // ignore storage exceptions here which are common as this is intended
          // to run while aktualizr sets up the storage
        }
        sleep(1);
      }
    } else {
      storage = INvStorage::newStorage(config.storage, readonly);
    }

    bool deviceid_loaded = false;
    if (storage->loadDeviceId(&device_id)) {
      deviceid_loaded = true;
      // Early return if only printing device ID.
      if (vm.count("name-only") != 0U) {
        std::cout << device_id << std::endl;
        return EXIT_SUCCESS;
      }
    }

    registered = registered || storage->loadEcuRegistered();
    has_metadata = has_metadata || storage->loadLatestRoot(&director_root, Uptane::RepositoryType::Director());

    bool tlscred_loaded = false;
    {
      std::string ca;
      std::string cert;
      std::string pkey;

      storage->loadTlsCreds(&ca, &cert, &pkey);
      if (!ca.empty() || !cert.empty() || !pkey.empty()) {
        tlscred_loaded = true;
      }
      // TLS credentials
      if (vm.count("tls-creds") != 0U) {
        std::cout << "Root CA certificate:" << std::endl << ca << std::endl;
        std::cout << "Client certificate:" << std::endl << cert << std::endl;
        std::cout << "Client private key:" << std::endl << pkey << std::endl;
        cmd_trigger = true;
      }
    }

    if (vm.count("tls-root-ca") != 0U) {
      std::string ca;
      storage->loadTlsCa(&ca);
      std::cout << ca << std::endl;
      cmd_trigger = true;
    }

    if (vm.count("tls-cert") != 0U) {
      std::string cert;
      storage->loadTlsCert(&cert);
      std::cout << cert << std::endl;
      cmd_trigger = true;
    }

    if (vm.count("tls-prv-key") != 0U) {
      std::string key;
      storage->loadTlsPkey(&key);
      std::cout << key << std::endl;
      cmd_trigger = true;
    }

    // ECU credentials
    bool ecukeys_loaded = false;
    std::string priv;
    std::string pub;

    storage->loadPrimaryKeys(&pub, &priv);
    if (!pub.empty() && !priv.empty()) {
      ecukeys_loaded = true;
    }
    if (vm.count("ecu-keys") != 0U) {
      std::cout << "Public key:" << std::endl << pub << std::endl;
      std::cout << "Private key:" << std::endl << priv << std::endl;
      cmd_trigger = true;
    }

    if (vm.count("ecu-pub-key") != 0U) {
      std::string key;
      storage->loadPrimaryPublic(&key);
      std::cout << key << std::endl;
      return EXIT_SUCCESS;
    }

    if (vm.count("ecu-prv-key") != 0U) {
      std::string key;
      storage->loadPrimaryPrivate(&key);
      std::cout << key << std::endl;
      cmd_trigger = true;
    }

    // An arguments which depend on metadata.
    std::string msg_metadata_fail = "Metadata is not available";
    if (vm.count("image-root") != 0U || vm.count("images-root") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::string images_root;
        storage->loadLatestRoot(&images_root, Uptane::RepositoryType::Image());
        std::cout << images_root << std::endl;
      }
      cmd_trigger = true;
    }

    if (vm.count("image-targets") != 0U || vm.count("image-target") != 0U || vm.count("images-targets") != 0U ||
        vm.count("images-target") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::string images_targets;
        storage->loadNonRoot(&images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
        std::cout << images_targets << std::endl;
      }
      cmd_trigger = true;
    }

    if (vm.count("delegation") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        loadAndPrintDelegations(storage);
      }
      cmd_trigger = true;
    }

    if (vm.count("director-root") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::cout << director_root << std::endl;
      }
      cmd_trigger = true;
    }

    if (vm.count("director-targets") != 0U || vm.count("director-target") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::string director_targets;
        storage->loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
        std::cout << director_targets << std::endl;
      }
      cmd_trigger = true;
    }

    if (vm.count("image-snapshot") != 0U || vm.count("images-snapshot") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::string snapshot;
        storage->loadNonRoot(&snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());
        std::cout << snapshot << std::endl;
      }
      cmd_trigger = true;
    }

    if (vm.count("image-timestamp") != 0U || vm.count("images-timestamp") != 0U) {
      if (!has_metadata) {
        std::cout << msg_metadata_fail << std::endl;
      } else {
        std::string timestamp;
        storage->loadNonRoot(&timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
        std::cout << timestamp << std::endl;
      }
      cmd_trigger = true;
    }

    if (cmd_trigger) {
      return EXIT_SUCCESS;
    }

    if (!deviceid_loaded && !tlscred_loaded && ecukeys_loaded) {
      secondary_db = true;
    }

    // Print general information if user does not provide any argument.
    if (!secondary_db) {
      if (!deviceid_loaded) {
        std::cout << "Couldn't load device ID" << std::endl;
      } else {
        std::cout << "Device ID: " << device_id << std::endl;
      }
    }

    std::string ecu_name = secondary_db ? "Secondary" : "Primary";
    EcuSerials serials;
    if (!storage->loadEcuSerials(&serials)) {
      std::cout << "Couldn't load ECU serials" << std::endl;
    } else if (serials.size() == 0) {
      std::cout << ecu_name << " serial is not found" << std::endl;
    } else {
      std::cout << ecu_name << " ECU serial ID: " << serials[0].first << std::endl;
      std::cout << ecu_name << " ECU hardware ID: " << serials[0].second << std::endl;
    }

    if (serials.size() > 1) {
      auto it = serials.begin() + 1;
      std::cout << "Secondaries:\n";
      int secondary_number = 1;
      for (; it != serials.end(); ++it) {
        std::cout << secondary_number++ << ") serial ID: " << it->first << std::endl;
        std::cout << "   hardware ID: " << it->second << std::endl;

        boost::optional<Uptane::Target> current_version;
        boost::optional<Uptane::Target> pending_version;

        auto load_installed_version_res =
            storage->loadInstalledVersions((it->first).ToString(), &current_version, &pending_version);

        if (!load_installed_version_res || (!current_version && !pending_version)) {
          std::cout << "   no details about installed nor pending images\n";
        } else {
          if (!!current_version) {
            std::cout << "   installed image hash: " << current_version->sha256Hash() << "\n";
            std::cout << "   installed image filename: " << current_version->filename() << "\n";
          }
          if (!!pending_version) {
            std::cout << "   pending image hash: " << pending_version->sha256Hash() << "\n";
            std::cout << "   pending image filename: " << pending_version->filename() << "\n";
          }
        }
      }
    }

    std::vector<MisconfiguredEcu> misconfigured_ecus;
    storage->loadMisconfiguredEcus(&misconfigured_ecus);
    if (misconfigured_ecus.size() != 0U) {
      std::cout << "Removed or unregistered ECUs (deprecated):" << std::endl;
      std::vector<MisconfiguredEcu>::const_iterator it;
      for (it = misconfigured_ecus.begin(); it != misconfigured_ecus.end(); ++it) {
        std::cout << "   '" << it->serial << "' with hardware_id '" << it->hardware_id << "' "
                  << (it->state == EcuState::kOld ? "has been removed from config" : "not registered yet") << std::endl;
      }
    }

    if (!secondary_db) {
      std::cout << "Provisioned on server: " << (registered ? "yes" : "no") << std::endl;
    }
    std::cout << "Fetched metadata: " << (has_metadata ? "yes" : "no") << std::endl;

    auto pacman = PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);

    Uptane::Target current_target = pacman->getCurrent();

    if (current_target.IsValid()) {
      std::cout << "Current " << ecu_name << " ECU running version: " << current_target.sha256Hash() << std::endl;
    } else {
      std::cout << "No currently running version on " << ecu_name << " ECU" << std::endl;
    }

    std::vector<Uptane::Target> installed_versions;
    boost::optional<Uptane::Target> pending;
    storage->loadPrimaryInstalledVersions(nullptr, &pending);

    if (!!pending) {
      std::cout << "Pending " << ecu_name << " ECU version: " << pending->sha256Hash() << std::endl;
    }
  } catch (const bpo::error &o) {
    std::cout << o.what() << std::endl << description;
    return EXIT_FAILURE;

  } catch (const std::exception &exc) {
    std::cerr << "Error: " << exc.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
