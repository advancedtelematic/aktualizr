#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <memory>
#include <thread>

#include "aktualizr_secondary.h"
#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_factory.h"
#include "utilities/aktualizr_version.h"
#include "utilities/utils.h"

#include "logging/logging.h"
#include "secondary_tcp_server.h"

namespace bpo = boost::program_options;

void check_secondary_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-secondary version is: " << aktualizr_version() << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("aktualizr-secondary command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr-secondary version")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("server-port,p", bpo::value<int>(), "command server listening port")
      ("ecu-serial", bpo::value<std::string>(), "serial number of Secondary ECU")
      ("ecu-hardware-id", bpo::value<std::string>(), "hardware ID of Secondary ECU");
  // clang-format on

  bpo::variables_map vm;
  std::vector<std::string> unregistered_options;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
    bpo::store(parsed_options, vm);
    check_secondary_options(description, vm);
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
    check_secondary_options(description, vm);

    // log boost error
    LOG_WARNING << "boost command line option error: " << ex.what();

    // print the error message to the standard output too, as the user provided
    // a non-supported commandline option
    std::cout << ex.what() << '\n';

    // set the returnValue, thereby ctest will recognize
    // that something went wrong
    exit(EXIT_FAILURE);
  }

  return vm;
}

/*****************************************************************************/
int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::info);
  LOG_INFO << "aktualizr-secondary version " << aktualizr_version() << " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int ret = EXIT_SUCCESS;
  try {
    AktualizrSecondaryConfig config(commandline_map);
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    auto secondary = AktualizrSecondaryFactory::create(config);
    SecondaryTcpServer tcp_server(secondary->getDispatcher(), config.network.primary_ip, config.network.primary_port,
                                  config.network.port, config.uptane.force_install_completion);

    tcp_server.run();

    if (tcp_server.exit_reason() == SecondaryTcpServer::ExitReason::kRebootNeeded) {
      secondary->completeInstall();
    }

  } catch (std::runtime_error &exc) {
    LOG_ERROR << "Error: " << exc.what();
    ret = EXIT_FAILURE;
  }
  return ret;
}
