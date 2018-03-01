#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <openssl/ssl.h>

#include "socket_activation.h"

#include "logging.h"

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-secondary version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("aktualizr-secondary command line options");
  // clang-format off
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr-secondary version")
      ("loglevel", bpo::value<int>(), "set log level 0-4 (trace, debug, warning, info, error)")
      ("config,c", bpo::value<std::string>()->required(), "toml configuration file");
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
    if (ex.get_option_name() == "--config") {
      std::cout << ex.get_option_name() << " is missing.\nYou have to provide a valid configuration "
                                           "file using toml format. See the example configuration file "
                                           "in config/aktualizr_secondary/config.toml.example"
                << std::endl;
      exit(EXIT_FAILURE);
    } else {
      // print the error and append the default commandline option description
      std::cout << ex.what() << std::endl << description;
      exit(EXIT_SUCCESS);
    }
  } catch (const bpo::error &ex) {
    check_info_options(description, vm);

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

  bpo::variables_map commandline_map = parse_options(argc, argv);

  // check for loglevel
  if (commandline_map.count("loglevel") != 0) {
    // set the log level from command line option
    boost::log::trivial::severity_level severity =
        static_cast<boost::log::trivial::severity_level>(commandline_map["loglevel"].as<int>());
    if (severity < boost::log::trivial::trace) {
      LOG_DEBUG << "Invalid log level";
      severity = boost::log::trivial::trace;
    }
    if (boost::log::trivial::fatal < severity) {
      LOG_WARNING << "Invalid log level";
      severity = boost::log::trivial::fatal;
    }
    if (severity <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    logger_set_threshold(severity);
  }

  LOG_INFO << "Aktualizr-secondary version " AKTUALIZR_VERSION " starting";
  LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();
  // Initialize config with default values, the update with config, then with cmd
  boost::filesystem::path secondary_config_path(commandline_map["config"].as<std::string>());
  if (!boost::filesystem::exists(secondary_config_path)) {
    std::cout << "aktualizr-secondary: configuration file " << boost::filesystem::absolute(secondary_config_path)
              << " not found. Exiting." << std::endl;
    exit(EXIT_FAILURE);
  }

  try {
    // socket activation
    int socket_fd;
    struct sockaddr_in6 sa;
    int port = 6666;

    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(port);
    sa.sin6_addr = IN6ADDR_ANY_INIT;

    if (socket_activation::listen_fds(0) == 1) {
      socket_fd = socket_activation::listen_fds_start;
    } else {
      // TODO: get from the config
      throw std::runtime_error("No socket found");
    }

    int v6only = 0;
    setsockopt(socket_fd, SOL_SOCKET, IPV6_V6ONLY, &v6only, sizeof(int));

    if (bind(socket_fd, reinterpret_cast<const sockaddr *>(&sa), sizeof(sa)) == -1) {
      throw std::runtime_error("bind failed");
    }
  } catch (std::runtime_error &exc) {
    LOG_ERROR << "Error: " << exc.what();

    return 1;
  }

  return 0;
}
