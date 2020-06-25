#include <unistd.h>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <libaktualizr/config.h>

#include "get.h"
#include "utilities/aktualizr_version.h"
#include "logging/logging.h"

namespace bpo = boost::program_options;

bpo::variables_map parse_options(int argc, char **argv) {
  bpo::options_description description(
      "A tool similar to wget that will do an HTTP get on the given URL using the device's configured credentials.");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("url,u", bpo::value<std::string>(), "url to get");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options = bpo::command_line_parser(argc, argv).options(description).run();
    bpo::store(parsed_options, vm);
    bpo::notify(vm);
    if (vm.count("help") != 0) {
      std::cout << description << '\n';
      exit(EXIT_SUCCESS);
    }

  } catch (const bpo::required_option &ex) {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_FAILURE);
  } catch (const bpo::error &ex) {
    std::cout << ex.what() << std::endl;
    std::cout << description;
    exit(EXIT_FAILURE);
  }

  return vm;
}

int main(int argc, char *argv[]) {
  logger_init(isatty(1) == 1);
  logger_set_threshold(boost::log::trivial::info);

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = EXIT_FAILURE;
  try {
    Config config(commandline_map);
    std::string body = aktualizrGet(config, commandline_map["url"].as<std::string>());
    std::cout << body;

    r = EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
  }
  return r;
}
