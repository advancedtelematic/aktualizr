#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "bootstrap.h"
#include "config.h"
#include "logger.h"
#include "utils.h"

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr_implicit_writer version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("aktualizr_implicit_writer command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr_implicit_writer version")
      ("credentials,c", bpo::value<std::string>()->required(), "zipped credentials file")
      ("config-input,i", bpo::value<std::string>()->required(), "input sota.toml configuration file")
      ("config-output,o", bpo::value<std::string>()->required(), "output sota.toml configuration file")
      ("prefix,p", bpo::value<std::string>(), "prefix for root CA output path")
      ("no-root-ca", "don't overwrite root CA path");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
    unregistered_options = bpo::collect_unrecognized(parsed_options.options, bpo::include_positional);
    if (vm.count("help") == 0 && !unregistered_options.empty()) {
      std::cout << description << "\n";
      exit(EXIT_FAILURE);
    }
  } catch (const bpo::required_option &ex) {
    // print the error and append the default commandline option description
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_SUCCESS);
  } catch (const bpo::error &ex) {
    check_info_options(description, vm);

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

int main(int argc, char *argv[]) {
  loggerInit();
  loggerSetSeverity(static_cast<LoggerLevels>(2));

  bpo::variables_map commandline_map = parse_options(argc, argv);

  std::string credentials_path = commandline_map["credentials"].as<std::string>();
  std::string config_in_path = commandline_map["config-input"].as<std::string>();
  std::string config_out_path = commandline_map["config-output"].as<std::string>();
  bool no_root = (commandline_map.count("no-root-ca") != 0);

  std::string prefix = "";
  if (commandline_map.count("prefix") != 0) {
    prefix = commandline_map["prefix"].as<std::string>();
  }

  std::cout << "Reading config file: " << config_in_path << "\n";
  Config config(config_in_path);

  if (!no_root) {
    Bootstrap boot(credentials_path, "");
    boost::filesystem::path ca_path(prefix);
    config.import.tls_cacert_path = "/usr/lib/sota/root.crt";
    ca_path /= config.import.tls_cacert_path;
    std::cout << "Writing root CA: " << ca_path << "\n";
    Utils::writeFile(ca_path.string(), boot.getCa());
  }

  config.tls.server = Bootstrap::readServerUrl(credentials_path);
  config.provision.server = config.tls.server;
  config.uptane.repo_server = config.tls.server + "/repo";
  config.uptane.director_server = config.tls.server + "/director";
  config.uptane.ostree_server = config.tls.server + "/treehub";

  std::cout << "Writing config file: " << config_out_path << "\n";
  boost::filesystem::path parent_dir = boost::filesystem::path(config_out_path).parent_path();
  if (!parent_dir.empty()) {
    boost::filesystem::create_directories(parent_dir);
  }
  config.writeToFile(config_out_path);

  return 0;
}
