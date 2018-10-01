#include "aktualizr.h"

#include <chrono>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

#include "utilities/timer.h"

using std::make_shared;
using std::shared_ptr;

Aktualizr::Aktualizr(Config &config) : config_(config) {
  systemSetup();
  sig_ = make_shared<boost::signals2::signal<void(shared_ptr<event::BaseEvent>)>>();
  storage_ = INvStorage::newStorage(config_.storage);
  storage_->importData(config_.import);
  uptane_client_ = SotaUptaneClient::newDefaultClient(config_, storage_, sig_);
}

Aktualizr::Aktualizr(Config &config, std::shared_ptr<INvStorage> storage_in,
                     std::shared_ptr<SotaUptaneClient> uptane_client_in, std::shared_ptr<event::Channel> sig_in)
    : config_(config) {
  systemSetup();
  storage_ = std::move(storage_in);
  uptane_client_ = std::move(uptane_client_in);
  sig_ = std::move(sig_in);
}

void Aktualizr::systemSetup() {
  if (sodium_init() == -1) {  // Note that sodium_init doesn't require a matching 'sodium_deinit'
    throw std::runtime_error("Unable to initialize libsodium");
  }

  LOG_TRACE << "Seeding random number generator from /dev/urandom...";
  Timer timer;
  unsigned int seed;
  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  urandom.read(reinterpret_cast<char *>(&seed), sizeof(seed));
  urandom.close();
  std::srand(seed);  // seeds pseudo random generator with random number
  LOG_TRACE << "... seeding complete in " << timer;
}

void Aktualizr::Initialize() { uptane_client_->initialize(); }

Aktualizr::CycleEventHandler::CycleEventHandler(Aktualizr &akt) : aktualizr(akt), fut(finished.get_future()) {}

void Aktualizr::CycleEventHandler::breakLoop() {
  std::lock_guard<std::mutex> l(m);
  if (!running) {
    return;
  }
  running = false;
  finished.set_value(true);
}

void Aktualizr::CycleEventHandler::handle(const std::shared_ptr<event::BaseEvent> &event) {
  if (!running) {
    return;
  }

  RunningMode running_mode = aktualizr.config_.uptane.running_mode;

  if (event->variant == "FetchMetaComplete") {
    aktualizr.CheckUpdates();
  } else if (event->variant == "NoUpdateAvailable") {
    breakLoop();
  } else if (event->variant == "UpdateAvailable") {
    auto ua_event = dynamic_cast<event::UpdateAvailable *>(event.get());
    assert(!ua_event->updates.empty());
    if (running_mode == RunningMode::kCheck) {
      breakLoop();
    } else if (running_mode == RunningMode::kInstall) {
      aktualizr.Install(ua_event->updates);
    } else {
      aktualizr.Download(ua_event->updates);
    }
  } else if (event->variant == "NothingToDownload") {
    breakLoop();
  } else if (event->variant == "DownloadProgressReport") {
    // silent
  } else if (event->variant == "AllDownloadsComplete") {
    auto dc_event = dynamic_cast<event::AllDownloadsComplete *>(event.get());
    if (running_mode == RunningMode::kDownload) {
      breakLoop();
    } else {
      aktualizr.Install(dc_event->updates);
    }
  } else if (event->variant == "AllInstallsComplete") {
    // finished installing everything
    aktualizr.uptane_client_->putManifest();
  } else if (event->variant == "PutManifestComplete") {
    boost::filesystem::path reboot_flag = "/tmp/aktualizr_reboot_flag";
    if (boost::filesystem::exists(reboot_flag)) {
      boost::filesystem::remove(reboot_flag);
      if (getppid() == 1) {  // if parent process id is 1, aktualizr runs under systemd
        exit(0);             // aktualizr service runs with 'Restart=always' systemd option.
        // Systemd will start aktualizr automatically if it exit.
      } else {  // Aktualizr runs from terminal
        LOG_INFO << "Aktualizr has been updated and requires restarting to run the new version.";
      }
    }
    breakLoop();
  } else if (event->variant == "Error") {
    auto err_event = dynamic_cast<event::Error *>(event.get());
    LOG_WARNING << "got Error event: " << err_event->message;
    breakLoop();
  }
}

void Aktualizr::UptaneCycle() {
  CycleEventHandler ev_handler(*this);

  std::function<void(std::shared_ptr<event::BaseEvent> event)> cb =
      [&ev_handler](const std::shared_ptr<event::BaseEvent> &event) { ev_handler.handle(event); };
  auto conn = SetSignalHandler(cb);

  // launch the cycle
  FetchMetadata();

  ev_handler.fut.wait();
  conn.disconnect();
}

int Aktualizr::Run() {
  SendDeviceData();
  while (!shutdown_) {
    UptaneCycle();
    std::this_thread::sleep_for(std::chrono::seconds(config_.uptane.polling_sec));
  }
  return EXIT_SUCCESS;
}

void Aktualizr::AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &secondary) {
  uptane_client_->addNewSecondary(secondary);
}

void Aktualizr::Shutdown() { shutdown_ = true; }

void Aktualizr::CampaignCheck() { uptane_client_->campaignCheck(); }

void Aktualizr::CampaignAccept(const std::string &campaign_id) { uptane_client_->campaignAccept(campaign_id); }

void Aktualizr::SendDeviceData() { uptane_client_->sendDeviceData(); }

void Aktualizr::FetchMetadata() { uptane_client_->fetchMeta(); }

void Aktualizr::CheckUpdates() { uptane_client_->checkUpdates(); }

void Aktualizr::Download(const std::vector<Uptane::Target> &updates) { uptane_client_->downloadImages(updates); }

void Aktualizr::Install(const std::vector<Uptane::Target> &updates) { uptane_client_->uptaneInstall(updates); }

boost::signals2::connection Aktualizr::SetSignalHandler(std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return sig_->connect(handler);
}
