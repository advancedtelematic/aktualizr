#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <memory>

#include <openssl/ssl.h>

#include "aktualizr_secondary.h"
#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_discovery.h"
#ifdef OPCUA_SECONDARY_ENABLED
#include "aktualizr_secondary_opcua.h"
#endif
#include "utilities/utils.h"

#include "logging/logging.h"

namespace bpo = boost::program_options;

class AktualizrSecondaryWithDiscovery : public AktualizrSecondaryInterface {
 public:
  AktualizrSecondaryWithDiscovery(const AktualizrSecondaryConfig &config, const std::shared_ptr<INvStorage> &storage)
      : aktualizr_secondary_(new AktualizrSecondary(config, storage)),
        discovery_(new AktualizrSecondaryDiscovery(config.network, *aktualizr_secondary_)) {}
  AktualizrSecondaryWithDiscovery(const AktualizrSecondaryWithDiscovery &) = delete;
  AktualizrSecondaryWithDiscovery &operator=(const AktualizrSecondaryWithDiscovery &) = delete;

  ~AktualizrSecondaryWithDiscovery() override {
    discovery_->stop();
    discovery_thread_.join();
  }
  void run() override {
    // run discovery service
    discovery_thread_ = std::thread(&AktualizrSecondaryDiscovery::run, discovery_.get());

    aktualizr_secondary_->run();
  }
  void stop() override { aktualizr_secondary_->stop(); }

 private:
  std::unique_ptr<AktualizrSecondary> aktualizr_secondary_;
  std::unique_ptr<AktualizrSecondaryDiscovery> discovery_;
  std::thread discovery_thread_;
};

#ifdef OPCUA_SECONDARY_ENABLED
typedef AktualizrSecondaryOpcua AktualizrSecondaryOpcuaWithDiscovery;
#endif

void check_secondary_options(const bpo::options_description &description, const bpo::variables_map &vm) {
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
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("server-port,p", bpo::value<int>(), "command server listening port")
      ("discovery-port,d", bpo::value<int>(), "discovery service listening port (0 to disable)")
      ("ecu-serial", bpo::value<std::string>(), "serial number of secondary ecu")
      ("ecu-hardware-id", bpo::value<std::string>(), "hardware ID of secondary ecu")
      ("opcua", "use OPC-UA protocol");
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
  LOG_INFO << "Aktualizr-secondary version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int ret = 0;
  try {
    AktualizrSecondaryConfig config(commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    // storage (share class with primary)
    std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

    std::unique_ptr<AktualizrSecondaryInterface> secondary;

    if (commandline_map.count("opcua") != 0) {
#ifdef OPCUA_SECONDARY_ENABLED
      secondary = std_::make_unique<AktualizrSecondaryOpcuaWithDiscovery>(config, storage);
#else
      LOG_ERROR << "Built without OPC-UA support!";
      return 1;
#endif
    } else {
      secondary = std_::make_unique<AktualizrSecondaryWithDiscovery>(config, storage);
    }

    secondary->run();

  } catch (std::runtime_error &exc) {
    LOG_ERROR << "Error: " << exc.what();
    ret = 1;
  }
  return ret;
}
