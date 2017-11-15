#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "ecuinterface.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  po::options_description desc("Allowed options");
  po::positional_options_description p;
  p.add("command", 4);
  // clang-format off
  desc.add_options()
    ("loglevel,l", po::value<unsigned int>()->default_value(0), "set log level 0-4 (trace, debug, warning, info, error)")
    ("command", po::value<std::vector<std::string> >(), "api-version | list-ecus | install-software");

  // clang-format on

  po::variables_map vm;

  try {
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);

    if (vm.count("command")) {
      std::vector<std::string> command = vm["command"].as<std::vector<std::string> >();

      ECUInterface ecu(vm["loglevel"].as<unsigned int>());
      if (command[0] == "api-version") {
        std::cout << ecu.apiVersion();
        return EXIT_SUCCESS;
      } else if (command[0] == "list-ecus") {
        std::cout << ecu.listEcus();
        return EXIT_SUCCESS;
      } else if (command[0] == "install-software") {
        if (command.size() != 4) {
          std::cout << "Wrong arguments count for command  install-software \n";
          return EXIT_FAILURE;
        }
        return ecu.installSoftware(command[1], command[2], command[3]);
      } else {
        std::cout << "unknown command: " << command[0] << "\n";
        std::cout << desc;
      }

      return EXIT_SUCCESS;
    } else {
      std::cout << "You must provide command: \n";
      std::cout << desc;
      return EXIT_FAILURE;
    }
    po::notify(vm);
  } catch (const po::error &o) {
    std::cout << o.what();
    std::cout << desc;
    return EXIT_FAILURE;
  }
}
// vim: set tabstop=2 shiftwidth=2 expandtab:
