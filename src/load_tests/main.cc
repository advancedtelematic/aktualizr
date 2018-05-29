#include <curl/curl.h>
#include <logging/logging.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>

#include "check.h"
#include "provision.h"
#include "sslinit.h"
#ifdef BUILD_OSTREE
#include "treehub.h"
#endif

namespace bpo = boost::program_options;

void checkForUpdatesCmd(const std::vector<std::string> &opts) {
  namespace fs = boost::filesystem;
  unsigned int devicesPerSec;
  unsigned int opsNr;
  unsigned int parallelism;
  std::string inputDir;
  bpo::options_description description("Check for update options");
  // clang-format off
  description.add_options()
      ("inputdir,i", bpo::value<std::string>(&inputDir)->required(), "path to the input data")
      ("rate,r", bpo::value<unsigned int>(&devicesPerSec)->default_value(5), "devices/sec")
      ("number,n", bpo::value<unsigned int>(&opsNr)->default_value(100), "number of operation to execute")
      ("threads,t", bpo::value<unsigned int>(&parallelism)->default_value(std::thread::hardware_concurrency()), "number of worker threads");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(opts).options(description).run(), vm);
  bpo::notify(vm);

  const fs::path inputPath(inputDir);
  LOG_INFO << "Checking for updates...";
  checkForUpdates(inputPath, devicesPerSec, opsNr, parallelism);
}

#ifdef BUILD_OSTREE
void fetchRemoteCmd(const std::vector<std::string> &opts) {
  std::string inputDir;
  std::string outDir;
  std::string branchName;
  std::string remoteUrl;
  unsigned int opsPerSec;
  unsigned int opsNr;
  unsigned int parallelism;
  bpo::options_description description("Fetch from ostree");
  // clang-format off
  description.add_options()
      ("inputdir,i", bpo::value<std::string>(&inputDir)->required(), "Directory containig provisioned devices.")
      ("outputdir,o", bpo::value<std::string>(&outDir)->required(), "Directory where repos will be created")
      ("branch,b", bpo::value<std::string>(&branchName)->required(), "Name of a branch to pull")
      ("url,u", bpo::value<std::string>(&remoteUrl)->required(), "Url of the repository")
      ("number,n", bpo::value<unsigned int>(&opsNr)->default_value(100), "number of operation to execute")
      ("threads,t", bpo::value<unsigned int>(&parallelism)->default_value(std::thread::hardware_concurrency()), "number of worker threads")
      ("rate,r", bpo::value<unsigned int>(&opsPerSec)->default_value(50), "repo pulls per second");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(opts).options(description).run(), vm);
  bpo::notify(vm);

  const boost::filesystem::path outputPath(outDir);
  fetchFromOstree(inputDir, outputPath, branchName, remoteUrl, opsPerSec, opsNr, parallelism);
}

void checkAndFetchCmd(const std::vector<std::string> &opts) {
  namespace fs = boost::filesystem;
  unsigned int checkPerSec;
  unsigned int checkNr;
  unsigned int checkParallelism;
  std::string branchName;
  std::string remoteUrl;
  unsigned int fetchesPerSec;
  unsigned int fetchesNr;
  unsigned int fetchesParallelism;
  std::string inputDir;
  std::string outDir;
  bpo::options_description description("Check for update options");
  // clang-format off
  description.add_options()
      ("inputdir,i", bpo::value<std::string>(&inputDir)->required(), "path to the input data")
      ("outputdir,o", bpo::value<std::string>(&outDir)->required(), "Directory where repos will be created")
      ("branch,b", bpo::value<std::string>(&branchName)->required(), "Name of a branch to pull")
      ("url,u", bpo::value<std::string>(&remoteUrl)->required(), "Url of the repository")
      ("fn", bpo::value<unsigned int>(&fetchesNr)->default_value(100), "number of fetches from treehub")
      ("ft", bpo::value<unsigned int>(&fetchesParallelism)->default_value(std::thread::hardware_concurrency()), "number of fetch worker threads")
      ("fr", bpo::value<unsigned int>(&fetchesPerSec)->default_value(50), "fetches per second")
      ("cr", bpo::value<unsigned int>(&checkPerSec)->default_value(5), "check for update/sec")
      ("cn", bpo::value<unsigned int>(&checkNr)->default_value(100), "number of checks to execute")
      ("ct", bpo::value<unsigned int>(&checkParallelism)->default_value(std::thread::hardware_concurrency()), "number of check worker threads");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::command_line_parser(opts).options(description).run(), vm);
  bpo::notify(vm);

  const fs::path inputPath(inputDir);
  const fs::path outputPath(outDir);
  std::thread checkThread{checkForUpdates, std::ref(inputPath), checkPerSec, checkNr, checkParallelism};
  std::thread fetchThread{
      fetchFromOstree, std::ref(inputPath), std::ref(outputPath), std::ref(branchName), std::ref(remoteUrl),
      fetchesPerSec,   fetchesNr,           fetchesParallelism};
  checkThread.join();
  fetchThread.join();
}
#endif

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

  std::map<std::string, std::function<void(std::vector<std::string>)>> commands{{"provision", provisionDevicesCmd},
                                                                                {"check", checkForUpdatesCmd}
#ifdef BUILD_OSTREE
                                                                                ,
                                                                                {"checkfetch", checkAndFetchCmd},
                                                                                {"fetch", fetchRemoteCmd}
#endif
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
  description.add_options()("help,h", "Show help message")("loglevel", bpo::value<int>()->default_value(3),
                                                           "set log level 0-4 (trace, debug, warning, info, error)")(
      "test", bpo::value<std::string>(&cmd), supportedTests.c_str());

  bpo::variables_map vm;
  bpo::parsed_options parsed = bpo::command_line_parser(argc, argv).options(description).allow_unregistered().run();
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
      std::vector<std::string> unprocessedOptions = bpo::collect_unrecognized(parsed.options, bpo::include_positional);
      fn->second(unprocessedOptions);
      openssl_callbacks_cleanup();
    } else {
      LOG_ERROR << supportedTests;
    }
  }
  return 0;
}