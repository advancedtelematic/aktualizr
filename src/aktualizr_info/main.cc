#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "config.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("aktualizr_info command line options");
  // clang-format off
  desc.add_options()
    ("help,h", "print usage")
    ("config,c", po::value<std::string>(), "toml configuration file")
    ("tls-creds",  "Outputs TLS credentials")
    ("ecu-keys",  "Outputs UPTANE keys")
    ("images-root",  "Outputs root.json from images repo")
    ("images-target",  "Outputs targets.json from images repo")
    ("director-root",  "Outputs root.json from director repo")
    ("director-target",  "Outputs targets.json from director repo");
  // clang-format on

  try {
    logger_init();
    logger_set_threshold(boost::log::trivial::error);
    po::variables_map vm;
    po::basic_parsed_options<char> parsed_options = po::command_line_parser(argc, argv).options(desc).run();
    po::store(parsed_options, vm);
    po::notify(vm);
    if (vm.count("help") != 0) {
      std::cout << desc << '\n';
      exit(EXIT_SUCCESS);
    }

    std::string sota_config_file = "/usr/lib/sota/sota.toml";
    if (vm.count("config") != 0) {
      sota_config_file = vm["config"].as<std::string>();
    } else if (boost::filesystem::exists("/tmp/aktualizr_config_path")) {
      sota_config_file = Utils::readFile("/tmp/aktualizr_config_path");
    }

    boost::filesystem::path sota_config_path(sota_config_file);
    if (!boost::filesystem::exists(sota_config_path)) {
      std::cout << "configuration file " << boost::filesystem::absolute(sota_config_path) << " not found. Exiting."
                << std::endl;
      exit(EXIT_FAILURE);
    }
    Config config(sota_config_path.string());

    std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
    std::cout << "Storage backend: " << ((storage->type() == kFileSystem) ? "Filesystem" : "Sqlite") << std::endl;

    Uptane::MetaPack pack;

    bool has_metadata = storage->loadMetadata(&pack);

    std::string device_id;
    if (!storage->loadDeviceId(&device_id)) {
      std::cout << "Couldn't load device ID" << std::endl;
    } else {
      std::cout << "Device ID: " << device_id << std::endl;
    }

    std::vector<std::pair<std::string, std::string> > serials;
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
      }
    }

    std::vector<MisconfiguredEcu> misconfigured_ecus;
    storage->loadMisconfiguredEcus(&misconfigured_ecus);
    if (misconfigured_ecus.size() != 0u) {
      std::cout << "Removed or not registered ecus:" << std::endl;
      std::vector<MisconfiguredEcu>::const_iterator it;
      for (it = misconfigured_ecus.begin(); it != misconfigured_ecus.end(); ++it) {
        std::cout << "   '" << it->serial << "' with hardware_id '" << it->hardware_id << "' "
                  << (it->state == kOld ? "has been removed from config" : "not registered yet") << std::endl;
      }
    }

    std::cout << "Provisioned on server: " << (storage->loadEcuRegistered() ? "yes" : "no") << std::endl;
    std::cout << "Fetched metadata: " << (has_metadata ? "yes" : "no") << std::endl;

    if (has_metadata) {
      if (vm.count("images-root") != 0u) {
        std::cout << "image root.json content:" << std::endl;
        std::cout << pack.image_root.toJson();
      }

      if (vm.count("images-target") != 0u) {
        std::cout << "image targets.json content:" << std::endl;
        std::cout << pack.image_targets.toJson();
      }

      if (vm.count("director-root") != 0u) {
        std::cout << "director root.json content:" << std::endl;
        std::cout << pack.director_root.toJson();
      }

      if (vm.count("director-target") != 0u) {
        std::cout << "director targets.json content:" << std::endl;
        std::cout << pack.director_targets.toJson();
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

  } catch (const po::error &o) {
    std::cout << o.what() << std::endl;
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
