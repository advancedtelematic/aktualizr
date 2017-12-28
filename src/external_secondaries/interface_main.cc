#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

#include "make_ecu.h"

namespace po = boost::program_options;
using std::string;

int main(int argc, char **argv) {
  po::options_description desc(
      "Usage: ecuinterface api-version | list-ecus | install-software [options]\nAllowed options");

  string command;
  string hardware_identifier;
  string ecu_identifier;
  boost::filesystem::path firmware_path;

  // clang-format off
  desc.add_options()
    ("help,h", "print usage")
    ("loglevel,l", po::value<unsigned int>()->default_value(0),
         "set log level 0-4 (trace, debug, warning, info, error)")
    ("hardware-identifier", po::value<string>(&hardware_identifier),
         "hardware identifier of the ECU where the update should be installed (e.g. rh850)")
    ("ecu-identifier", po::value<string>(&ecu_identifier),
         "unique serial number of the ECU where the update should be installed (e.g. ‘abcdef12345’)")
    ("firmware", po::value<boost::filesystem::path>(&firmware_path),
         "absolute path to the firmware image to be installed.")
    ("command", po::value<string>(&command),
         "command to run: api-version | list-ecus | install-software");
  // clang-format on

  po::positional_options_description positional_options;
  positional_options.add("command", 1);

  try {
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_options).run(), vm);
    po::notify(vm);
    if (vm.count("help") != 0) {
      std::cout << desc << '\n';
      exit(EXIT_SUCCESS);
    }

    if (vm.count("command")) {
      ECUInterface *ecu = make_ecu(vm["loglevel"].as<unsigned int>());
      if (command == "api-version") {
        std::cout << ecu->apiVersion();
        return EXIT_SUCCESS;
      } else if (command == "list-ecus") {
        std::cout << ecu->listEcus();
        return EXIT_SUCCESS;
      } else if (command == "install-software") {
        if (!vm.count("hardware-identifier") || !vm.count("firmware")) {
          std::cerr << "install-software command requires --hardware-identifier, --firmware, and possibly "
                       "--ecu-identifier.\n";
          return EXIT_FAILURE;
        }
        ECUInterface::InstallStatus result =
            ecu->installSoftware(hardware_identifier, ecu_identifier, firmware_path.string());
        std::cout << "Installation result: " << result << "\n";
        return result;
      } else {
        std::cout << "unknown command: " << command[0] << "\n";
        std::cout << desc;
      }

      return EXIT_SUCCESS;
    } else {
      std::cout << "You must provide a command.\n";
      std::cout << desc;
      return EXIT_FAILURE;
    }
  } catch (const po::error &o) {
    std::cout << o.what();
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
