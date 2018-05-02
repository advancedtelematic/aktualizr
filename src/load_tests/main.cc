#include <iostream>
#include <boost/program_options.hpp>
#include <curl/curl.h>
#include <thread>
#include <logging/logging.h>

#include "provision.h"
#include "check.h"
#include "sslinit.h"

namespace bpo = boost::program_options;

void checkForUpdatesCmd(const std::vector<std::string> &opts) {
  namespace bpo = boost::program_options;
  namespace fs = boost::filesystem;
  uint devicesPerSec;
  uint opsNr;
  uint parallelism;
  std::string inputDir;
  std::string outDir;
  bpo::options_description description("Check for update options");
  // clang-format off
  description.add_options()
    ("inputdir,i", bpo::value<std::string>(&inputDir)->required(), "path to the input data")
    ("rate,r", bpo::value<uint>(&devicesPerSec)->default_value(5), "devices/sec")
    ("number,n", bpo::value<uint>(&opsNr)->default_value(100), "number of operation to execute")
    ("threads,t", bpo::value<uint>(&parallelism)->default_value(std::thread::hardware_concurrency()), "number of worker threads");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(opts).options(description).run(), vm);
  bpo::notify(vm);

  const fs::path inputPath(inputDir);
  LOG_INFO << "Checking for updates...";
  checkForUpdates(inputPath, devicesPerSec, opsNr, parallelism);
}

void provisionDevicesCmd(const std::vector<std::string> &opts) {
  using namespace boost::filesystem;
  size_t devicesNr;
  size_t parallelism;
  uint devicesPerSec;
  std::string outDir;
  std::string pathToCredentials;
  std::string gwUrl;
  bpo::options_description description("Register devices");

  // clang-format off
  description.add_options()
    ("outputdir,o", bpo::value<std::string>(&outDir)->required(), "output directory")
    ("gateway,g", bpo::value<std::string>(&gwUrl)->required(), "url of the device gateway")
    ("credentials,c", bpo::value<std::string>(&pathToCredentials), "path to a provisioning credentials")
    ("dev-number,n", bpo::value<size_t>(&devicesNr)->default_value(100), "number of devices")
    ("rate,r", bpo::value<uint>(&devicesPerSec)->default_value(2), "devices/sec")
    ("threads,t", bpo::value<size_t>(&parallelism)->default_value(std::thread::hardware_concurrency()), "number of worker threads");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(opts).options(description).run(), vm);
  bpo::notify(vm);

  const path devicesDir(outDir);

  const path credentialsFile(pathToCredentials);
  mkDevices(devicesDir, credentialsFile, gwUrl, parallelism, devicesNr, devicesPerSec);
}

void setLogLevel(const bpo::variables_map &vm) {
  // set the log level from command line option
  boost::log::trivial::severity_level severity =
    static_cast<boost::log::trivial::severity_level>(vm["loglevel"].as<int>());
  if (severity < boost::log::trivial::trace) {
    LOG_DEBUG << "Invalid log level";
    severity = boost::log::trivial::trace;
  }
  if (boost::log::trivial::fatal < severity) {
    LOG_WARNING << "Invalid log level";
    severity = boost::log::trivial::fatal;
  }
  LoggerConfig loggerConfig{};
  loggerConfig.loglevel = severity;
  logger_set_threshold(loggerConfig);
}

int main(int argc, char *argv[]) {
  std::srand(std::time(0));

  std::map<std::string, std::function<void(std::vector<std::string>)>> commands{
      // {"sla", slaCheckCmd},
      {"provision", provisionDevicesCmd},
      {"check", checkForUpdatesCmd}
      // {"pull", pullOSTreeCmd}
      };

  std::stringstream acc;
  bool first = true;
  acc << "Supported tests: ";
  for (auto const &elem : commands) {
    if (!first) {
      acc << ", ";
    } else {
      first = false;
    };
    acc << elem.first;
  };
  std::string supportedTests = acc.str();
  std::string cmd;
  bpo::options_description description("OTA load tests");
  description.add_options()("help,h", "Show help message")(
      "loglevel", bpo::value<int>()->default_value(3),
      "set log level 0-4 (trace, debug, warning, info, error)")(
      "test", bpo::value<std::string>(&cmd), supportedTests.c_str());

  bpo::variables_map vm;
  bpo::parsed_options parsed = bpo::command_line_parser(argc, argv)
                                   .options(description)
                                   .allow_unregistered()
                                   .run();
  bpo::store(parsed, vm);
  bpo::notify(vm);

  if (vm.count("help")) {
    std::cout << description << std::endl;
    return 0;
  }

  logger_init();
  setLogLevel(&vm);

  if (vm.count("test")) {
    auto fn = commands.find(cmd);
    if (fn != commands.end()) {
      openssl_callbacks_setup();
      std::vector<std::string> unprocessedOptions =
          bpo::collect_unrecognized(parsed.options, bpo::include_positional);
      fn->second(unprocessedOptions);
      openssl_callbacks_cleanup();
    } else {
      LOG_ERROR << supportedTests;
    }
  }
  return 0;
}