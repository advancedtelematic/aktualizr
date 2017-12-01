#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "config.h"
#include "fsstorage.h"
#include "logger.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("Allowed options");
  // clang-format off
  desc.add_options()
    ("config,c", po::value<std::string>()->default_value("/usr/lib/sota/sota.toml"), "toml configuration file")
    ("images-root",  "Outputs root.json from images repo")
    ("images-target",  "Outputs targets.json from images repo")
    ("director-root",  "Outputs root.json from director repo")
    ("director-target",  "Outputs targets.json from director repo");
  // clang-format on

  try {
    loggerSetSeverity(LVL_error);
    po::variables_map vm;
    po::basic_parsed_options<char> parsed_options = po::command_line_parser(argc, argv).options(desc).run();
    po::store(parsed_options, vm);
    po::notify(vm);

    std::string sota_config_file = vm["config"].as<std::string>();
    boost::filesystem::path sota_config_path(sota_config_file);
    if (false == boost::filesystem::exists(sota_config_path)) {
      std::cout << "configuration file " << boost::filesystem::absolute(sota_config_path) << " not found. Exiting."
                << std::endl;
      exit(EXIT_FAILURE);
    }
    Config config(sota_config_path.string());

    FSStorage storage(config.storage);
    Uptane::MetaPack pack;

    bool has_metadata = storage.loadMetadata(&pack);

    std::string device_id;
    if (!storage.loadDeviceId(&device_id)) {
      std::cout << "Device ID: " << device_id << std::endl;
    } else {
      std::cout << "Couldn't load device ID" << std::endl;
    }
    
    std::vector<std::pair<std::string, std::string> > serials;
    if (!storage.loadEcuSerials(&serials)) {
      std::cout << "Couldn't load ECU serials" << std::endl;
    } else if (serials.size() == 0) {
      std::cout << "Primary serial is not found" << std::endl;
    } else {
      std::cout << "Primary ecu serial ID: " << serials[0].first << std::endl;
    }

    std::cout << "Provisioned on server: " << (storage.loadEcuRegistered() ? "yes" : "no") << std::endl;
    std::cout << "Fetched metadata: " << (has_metadata ? "yes" : "no") << std::endl;

    if (has_metadata) {
      if (vm.count("images-root")) {
        std::cout << "image root.json content:" << std::endl;
        std::cout << pack.image_root.toJson();
      }

      if (vm.count("images-target")) {
        std::cout << "image targets.json content:" << std::endl;
        std::cout << pack.image_targets.toJson();
      }

      if (vm.count("director-root")) {
        std::cout << "director root.json content:" << std::endl;
        std::cout << pack.director_root.toJson();
      }

      if (vm.count("director-target")) {
        std::cout << "director targets.json content:" << std::endl;
        std::cout << pack.director_targets.toJson();
      }
    }

  } catch (const po::error &o) {
    std::cout << o.what() << std::endl;
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
