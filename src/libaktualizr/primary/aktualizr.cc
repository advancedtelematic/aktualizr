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

void Aktualizr::UptaneCycle() {
  RunningMode running_mode = config_.uptane.running_mode;
  UpdateCheckResult update_result = CheckUpdates();
  if (running_mode == RunningMode::kCheck || update_result.updates.size() == 0) {
    return;
  } else if (running_mode == RunningMode::kInstall) {
    Install(update_result.updates);
    uptane_client_->putManifest();
    return;
  }

  bool result;
  std::vector<Uptane::Target> downloaded_updates;
  std::tie(result, downloaded_updates) = Download(update_result.updates);
  if (running_mode == RunningMode::kDownload || !result || downloaded_updates.size() == 0) {
    return;
  }

  Install(downloaded_updates);
  uptane_client_->putManifest();
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

CampaignCheckResult Aktualizr::CampaignCheck() { return uptane_client_->campaignCheck(); }

void Aktualizr::CampaignAccept(const std::string &campaign_id) { uptane_client_->campaignAccept(campaign_id); }

void Aktualizr::SendDeviceData() { uptane_client_->sendDeviceData(); }

UpdateCheckResult Aktualizr::CheckUpdates() { return uptane_client_->fetchMeta(); }

std::pair<bool, std::vector<Uptane::Target>> Aktualizr::Download(const std::vector<Uptane::Target> &updates) {
  return uptane_client_->downloadImages(updates);
}

void Aktualizr::Install(const std::vector<Uptane::Target> &updates) { uptane_client_->uptaneInstall(updates); }

boost::signals2::connection Aktualizr::SetSignalHandler(std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return sig_->connect(handler);
}
