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
#include "utilities/aktualizr_version.h"
#include "utilities/utils.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;
using aktualizr_grpc::HmiServer;
using aktualizr_grpc::Campaign;
using aktualizr_grpc::CheckRequest;
using aktualizr_grpc::CheckResponse;
using aktualizr_grpc::DownloadRequest;
using aktualizr_grpc::DownloadResponse;
using aktualizr_grpc::InstallRequest;
using aktualizr_grpc::InstallResponse;

namespace bpo = boost::program_options;

void check_info_options(const bpo::options_description &description, const bpo::variables_map &vm) {
  if (vm.count("help") != 0) {
    std::cout << description << '\n';
    exit(EXIT_SUCCESS);
  }
  if (vm.count("version") != 0) {
    std::cout << "Current aktualizr-grpc-srv version is: " << aktualizr_version() << "\n";
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


class HmiServerImpl final : public HmiServer::Service {
  public:
    explicit HmiServerImpl(Config& config) :
      aktualizr_(config) { aktualizr_.Initialize(); }

    Status Check(ServerContext* context,
        const CheckRequest* request,
        CheckResponse* response) override {
      (void)request;
      (void)context;
      try {
        aktualizr_.SendDeviceData().get();
        auto campaigns = aktualizr_.CampaignCheck().get().campaigns;
        for (const auto& campaign : campaigns) {
          auto c = response->add_campaigns();
          c->set_id(campaign.id);
          c->set_name(campaign.name);
          c->set_description(campaign.description);
          c->set_size(campaign.size);
          c->set_auto_accept(campaign.autoAccept);
          c->set_install_sec(campaign.estInstallationDuration);
          c->set_prepare_sec(campaign.estPreparationDuration);
        }
      } catch (const std::exception &ex) {
        LOG_ERROR << "RPC Check error: " << ex.what();
        return Status(StatusCode::INTERNAL, ex.what(), "");
      }
      return Status::OK;
    }

    Status Download(ServerContext* context, const DownloadRequest* request,
        ServerWriter<DownloadResponse>* writer) {
      (void)context;
      try {
        aktualizr_.CampaignControl(request->campaign_id(), campaign::Cmd::Accept).get();
        auto updates = aktualizr_.CheckUpdates().get();
        if (updates.status != result::UpdateStatus::kUpdatesAvailable)
          throw(std::runtime_error("No updates found for the campaign"));

        std::function<void(std::shared_ptr<event::BaseEvent>)> handler = 
          [writer](std::shared_ptr<event::BaseEvent> event) {
            if (event->variant == "DownloadProgressReport") {
              const auto report =
                dynamic_cast<event::DownloadProgressReport *>(event.get());
              DownloadResponse r;
              r.set_progress(report->progress);
              writer->Write(r);
            }
          };
        auto conn = aktualizr_.SetSignalHandler(handler);

        auto download = aktualizr_.Download(updates.updates).get();
        conn.disconnect(); // TODO no disconnect on exception in Download
        if (download.status != result::DownloadStatus::kSuccess)
          throw(std::runtime_error("Download failed"));

        install_targets_ = download.updates;

      } catch (const std::exception &ex) {
        LOG_ERROR << "RPC Download error: " << ex.what();
        return Status(StatusCode::INTERNAL, ex.what(), "");
      }
      return Status::OK;
    }

    Status Install(ServerContext* context, const InstallRequest* request,
        InstallResponse* response) {
      (void)context;
      (void)request;
      (void)response;
      try {
        auto result = aktualizr_.Install(install_targets_).get();
        for (const auto& target : result.ecu_reports) {
          if (!target.install_res.isSuccess())
            throw(std::runtime_error("Installation failed"));
        }
        aktualizr_.SendDeviceData().get();
      } catch (const std::exception &ex) {
        LOG_ERROR << "RPC Install error: " << ex.what();
        return Status(StatusCode::INTERNAL, ex.what(), "");
      }
      return Status::OK;
    }

  private:
    Aktualizr aktualizr_;
    std::vector<Uptane::Target> install_targets_;
};

int main(int argc, char *argv[]) {
  try {
    logger_init();
    logger_set_threshold(boost::log::trivial::trace);
    LOG_INFO << "Aktualizr gRPC server version " << aktualizr_version() << " starting";

    bpo::variables_map commandline_map = parse_options(argc, argv);
    Config config(commandline_map);
    LOG_DEBUG << "Current directory: " << boost::filesystem::current_path().string();

    HmiServerImpl service(config);
    std::string server_address("0.0.0.0:50051");
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();

  } catch (const std::exception &ex) {
    LOG_ERROR << "Fatal error in aktualizr: " << ex.what();
    return -1;
  }

  return 0;
}
