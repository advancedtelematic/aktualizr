#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <openssl/ssl.h>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/signals2.hpp>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>

#include "aktualizr.grpc.pb.h"
#include "config/config.h"
#include "logging/logging.h"
#include "primary/aktualizr.h"
#include "uptane/secondaryfactory.h"
#include "utilities/utils.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using aktualizr::DownloadResponse;
using aktualizr::Campaign;

namespace bpo = boost::program_options;

std::map<std::string, unsigned int> progress;
std::mutex pending_ecus_mutex;
unsigned int pending_ecus;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-grpc-srv version is: " << AKTUALIZR_VERSION << "\n";
    exit(EXIT_SUCCESS);
  }
}

bpo::variables_map parse_options(int argc, char *argv[]) {
  bpo::options_description description("gRPC server for libaktualizr");
  // clang-format off
  // Try to keep these options in the same order as Config::updateFromCommandLine().
  // The first three are commandline only.
  description.add_options()
      ("help,h", "print usage")
      ("version,v", "Current aktualizr version")
      ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration file or directory")
      ("secondary-configs-dir", bpo::value<boost::filesystem::path>(), "directory containing seconday ECU configuration files")
      ("loglevel", bpo::value<int>(), "set log level 0-5 (trace, debug, info, warning, error, fatal)");
  // clang-format on

  bpo::variables_map vm;
  try {
    bpo::basic_parsed_options<char> parsed_options =
        bpo::command_line_parser(argc, argv).options(description).run();
    bpo::store(parsed_options, vm);
    check_info_options(description, vm);
    bpo::notify(vm);
  } catch (const bpo::required_option &ex) {
    std::cout << ex.what() << std::endl << description;
    exit(EXIT_FAILURE);
  } catch (const bpo::error &ex) {
    check_info_options(description, vm);
    LOG_ERROR << "boost command line option error: " << ex.what();
    std::cout << ex.what() << '\n';
    exit(EXIT_FAILURE);
  }

  return vm;
}

void process_event(const std::shared_ptr<event::BaseEvent> &event) {
  (void)event;
}


class AktualizrImpl final : public aktualizr::Aktualizr::Service {
 public:
  explicit AktualizrImpl() {}

  Status Check() {
    return Status::OK;
  }

  Status Download(std::string campaign_id) {
    (void)campaign_id;
    return Status::OK;
  }

  Status Install(std::string campaign_id) {
    (void)campaign_id;
    return Status::OK;
  }

};

int main(int argc, char *argv[]) {
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  LOG_INFO << "Aktualizr gRPC server version " AKTUALIZR_VERSION " starting";

  bpo::variables_map commandline_map = parse_options(argc, argv);

  int r = -1;
  boost::signals2::connection conn;

  try {
    Config config(commandline_map);
    if (config.logger.loglevel <= boost::log::trivial::debug) {
      SSL_load_error_strings();
    }
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    Aktualizr aktualizr(config);
    std::function<void(const std::shared_ptr<event::BaseEvent> event)> f_cb =
        [](const std::shared_ptr<event::BaseEvent> event) { process_event(event); };
    conn = aktualizr.SetSignalHandler(f_cb);

    aktualizr.Initialize();
    AktualizrImpl service;
    std::string server_address("0.0.0.0:50051");
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

    aktualizr.Shutdown();
    r = 0;
  } catch (const std::exception &ex) {
    LOG_ERROR << "Fatal error in hmi_stub: " << ex.what();
  }

  conn.disconnect();
  return r;
}
