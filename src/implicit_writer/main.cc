#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "bootstrap/bootstrap.h"
#include "config/config.h"
#include "logging/logging.h"
#include "utilities/utils.h"

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
  bpo::options_description description("aktualizr-implicit-writer command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr_implicit_writer version")
      ("credentials,c", bpo::value<boost::filesystem::path>()->required(), "zipped credentials file")
      ("config-output,o", bpo::value<boost::filesystem::path>()->required(), "output sota.toml configuration file")
      ("root-ca-path,r", bpo::value<boost::filesystem::path>()->default_value("/usr/lib/sota/root.crt"), "root CA output path")
      ("prefix,p", bpo::value<boost::filesystem::path>(), "installation prefix for root CA output path")
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
    exit(EXIT_FAILURE);
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
  logger_init();
  logger_set_threshold(static_cast<boost::log::trivial::severity_level>(2));

  // Suppress the following clang-tidy warning:
  // /usr/include/boost/detail/basic_pointerbuf.hpp
  // error: Call to virtual function during construction
  // [clang-analyzer-optin.cplusplus.VirtualCall,-warnings-as-errors]
  // basic_pointerbuf() : base_type() { setbuf(0, 0); }
  // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
  bpo::variables_map commandline_map = parse_options(argc, argv);

  boost::filesystem::path credentials_path = commandline_map["credentials"].as<boost::filesystem::path>();
  boost::filesystem::path config_out_path = commandline_map["config-output"].as<boost::filesystem::path>();
  boost::filesystem::path cacert_path = commandline_map["root-ca-path"].as<boost::filesystem::path>();
  const bool no_root = (commandline_map.count("no-root-ca") != 0);

  boost::filesystem::path prefix = "";
  if (commandline_map.count("prefix") != 0) {
    prefix = commandline_map["prefix"].as<boost::filesystem::path>();
  }

  std::cout << "Writing config file: " << config_out_path << "\n";
  boost::filesystem::path parent_dir = config_out_path.parent_path();
  if (!parent_dir.empty()) {
    boost::filesystem::create_directories(parent_dir);
  }
  std::ofstream sink(config_out_path.c_str(), std::ofstream::out);
  sink << "[tls]\n";
  sink << "server = \"" << Bootstrap::readServerUrl(credentials_path) << "\"\n\n";

  if (!no_root) {
    sink << "[import]\n";
    sink << "tls_cacert_path = \"" << cacert_path.string() << "\"\n\n";

    std::string server_ca = Bootstrap::readServerCa(credentials_path);
    if (server_ca.empty()) {
      Bootstrap boot(credentials_path, "");
      server_ca = boot.getCa();
      std::cout << "Server root CA read from autoprov_credentials.p12 in zipped archive.\n";
    } else {
      std::cout << "Server root CA read from server_ca.pem in zipped archive.\n";
    }

    boost::filesystem::path ca_path(prefix / cacert_path);
    std::cout << "Writing root CA: " << ca_path << "\n";
    Utils::writeFile(ca_path, server_ca);
  }

  return 0;
}
