#include <unistd.h>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "storage/invstorage.h"

#include "get.h"
#include "libaktualizr/config.h"
#include "logging/logging.h"
#include "utilities/aktualizr_version.h"

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0 || (vm.count("url") == 0 && vm.count("version") == 0)) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-get version is: " << aktualizr_version() << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char **argv) {
  bpo::options_description description(
      "A tool similar to wget that will do an HTTP get on the given URL using the device's configured credentials.");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr-get version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory, by default /var/sota")
      ("header,H", bpo::value<std::vector<std::string> >()->composing(), "Additional headers to pass")
      ("storage,s", bpo::value<std::string>(), "options: TUF(default), Uptane")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("url,u", bpo::value<std::string>(), "url to get, mandatory");

  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options = bpo::command_line_parser(argc, argv).options(description).run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
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
    std::vector<std::string> headers;
    if (commandline_map.count("header") == 1) {
      headers = commandline_map["header"].as<std::vector<std::string>>();
    }
    StorageClient client = StorageClient::kTUF;
    if (commandline_map.count("storage") == 1) {
      if (commandline_map["storage"].as<std::string>() == "Uptane") {
        client = StorageClient::kUptane;
      }
    }
    std::string body = aktualizrGet(config, commandline_map["url"].as<std::string>(), headers, client);
    std::cout << body;

    r = EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    LOG_ERROR << ex.what();
  }
  return r;
}
