#include "aktualizr.h"

#include <chrono>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

#include "primary/events.h"
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

Aktualizr::Aktualizr(Config &config, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in)
    : config_(config) {
  systemSetup();
  sig_ = make_shared<boost::signals2::signal<void(shared_ptr<event::BaseEvent>)>>();
  storage_ = std::move(storage_in);
  std::shared_ptr<Bootloader> bootloader_in = std::make_shared<Bootloader>(config_.bootloader, *storage_);
  std::shared_ptr<ReportQueue> report_queue_in = std::make_shared<ReportQueue>(config_, http_in);

  uptane_client_ = std::make_shared<SotaUptaneClient>(config_, storage_, http_in, bootloader_in, report_queue_in, sig_);
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

void Aktualizr::Initialize() {
  uptane_client_->initialize();
  api_queue_.run();
}

bool Aktualizr::UptaneCycle() {
  result::UpdateCheck update_result = CheckUpdates().get();
  if (update_result.updates.empty()) {
    return true;
  }

  result::Download download_result = Download(update_result.updates).get();
  if (download_result.status != result::DownloadStatus::kSuccess || download_result.updates.empty()) {
    return true;
  }

  Install(download_result.updates).get();

  if (uptane_client_->isInstallCompletionRequired()) {
    // If there are some pending updates then effectively either reboot (ostree) or aktualizr restart (fake pack mngr)
    // is required to apply the update(s)
    LOG_INFO << "About to exit aktualizr so the pending updates can be applied after reboot";
    return false;
  }

  if (!uptane_client_->hasPendingUpdates()) {
    // If updates were applied and no any reboot/finalization is required then send/put manifest
    // as soon as possible, don't wait for config_.uptane.polling_sec
    SendManifest().get();
  }

  return true;
}

std::future<void> Aktualizr::RunForever() {
  std::future<void> future = std::async(std::launch::async, [&]() {
    SendDeviceData().get();
    while (UptaneCycle()) {
      std::this_thread::sleep_for(std::chrono::seconds(config_.uptane.polling_sec));
    }
    uptane_client_->completeInstall();
  });
  return future;
}

void Aktualizr::AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &secondary) {
  uptane_client_->addNewSecondary(secondary);
}

std::future<result::CampaignCheck> Aktualizr::CampaignCheck() {
  std::function<result::CampaignCheck()> task([this] { return uptane_client_->campaignCheck(); });
  return api_queue_.enqueue(task);
}

std::future<void> Aktualizr::CampaignControl(const std::string &campaign_id, campaign::Cmd cmd) {
  std::function<void()> task([this, campaign_id, cmd] {
    switch (cmd) {
      case campaign::Cmd::Accept:
        uptane_client_->campaignAccept(campaign_id);
        break;
      case campaign::Cmd::Decline:
        uptane_client_->campaignDecline(campaign_id);
        break;
      case campaign::Cmd::Postpone:
        uptane_client_->campaignPostpone(campaign_id);
        break;
      default:
        break;
    }
  });
  return api_queue_.enqueue(task);
}

std::future<void> Aktualizr::SendDeviceData() {
  std::function<void()> task([this] { uptane_client_->sendDeviceData(); });
  return api_queue_.enqueue(task);
}

std::future<result::UpdateCheck> Aktualizr::CheckUpdates() {
  std::function<result::UpdateCheck()> task([this] { return uptane_client_->fetchMeta(); });
  return api_queue_.enqueue(task);
}

std::future<result::Download> Aktualizr::Download(const std::vector<Uptane::Target> &updates) {
  std::function<result::Download(const api::FlowControlToken *)> task(
      [this, updates](const api::FlowControlToken *token) { return uptane_client_->downloadImages(updates, token); });
  return api_queue_.enqueue(task);
}

std::future<result::Install> Aktualizr::Install(const std::vector<Uptane::Target> &updates) {
  std::function<result::Install()> task([this, updates] { return uptane_client_->uptaneInstall(updates); });
  return api_queue_.enqueue(task);
}

std::future<bool> Aktualizr::SendManifest(const Json::Value &custom) {
  std::function<bool()> task([this, custom]() { return uptane_client_->putManifest(custom); });
  return api_queue_.enqueue(task);
}

result::Pause Aktualizr::Pause() {
  if (api_queue_.pause(true)) {
    uptane_client_->reportPause();
    return result::PauseStatus::kSuccess;
  } else {
    return result::PauseStatus::kAlreadyPaused;
  }
}

result::Pause Aktualizr::Resume() {
  if (api_queue_.pause(false)) {
    uptane_client_->reportResume();
    return result::PauseStatus::kSuccess;
  } else {
    return result::PauseStatus::kAlreadyRunning;
  }
}

void Aktualizr::Abort() { api_queue_.abort(); }

boost::signals2::connection Aktualizr::SetSignalHandler(
    const std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return sig_->connect(handler);
}

std::vector<Uptane::Target> Aktualizr::GetStoredTargets() { return storage_->getTargetFiles(); }

void Aktualizr::DeleteStoredTarget(const Uptane::Target &target) { storage_->removeTargetFile(target.filename()); }

std::unique_ptr<StorageTargetRHandle> Aktualizr::OpenStoredTarget(const Uptane::Target &target) {
  auto handle = storage_->openTargetFile(target);
  if (handle->isPartial()) {
    throw std::runtime_error("Target was partially downloaded");
  }
  return handle;
}
