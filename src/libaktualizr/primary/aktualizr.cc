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

Aktualizr::Aktualizr(Config &config, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in)
    : config_(config) {
  systemSetup();
  sig_ = make_shared<boost::signals2::signal<void(shared_ptr<event::BaseEvent>)>>();
  storage_ = std::move(storage_in);
  uptane_client_ = SotaUptaneClient::newTestClient(config_, storage_, std::move(http_in), sig_);
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
  if (update_result.updates.size() == 0) {
    return true;
  }

  result::Download download_result = Download(update_result.updates).get();
  if (download_result.status != result::DownloadStatus::kSuccess || download_result.updates.size() == 0) {
    return true;
  }

  uptane_client_->uptaneInstall(download_result.updates);

  if (uptane_client_->isInstallCompletionRequired()) {
    // If there are some pending updates then effectively either reboot (ostree) or aktualizr restart (fake pack mngr)
    // is required to apply the update(s)
    LOG_INFO << "About to exit aktualizr so the pending updates can be applied after reboot";
    return false;
  }

  if (!uptane_client_->hasPendingUpdates()) {
    // If updates were applied and no any reboot/finalization is required then send/put manifest
    // as soon as possible, don't wait for config_.uptane.polling_sec
    uptane_client_->putManifest();
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

std::future<void> Aktualizr::CampaignAccept(const std::string &campaign_id) {
  std::function<void()> task([this, &campaign_id] { uptane_client_->campaignAccept(campaign_id); });
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
      [this, &updates](const api::FlowControlToken *token) { return uptane_client_->downloadImages(updates, token); });
  return api_queue_.enqueue(task);
}

std::future<result::Install> Aktualizr::Install(const std::vector<Uptane::Target> &updates) {
  std::function<result::Install()> task([this, &updates] { return uptane_client_->uptaneInstall(updates); });
  return api_queue_.enqueue(task);
}

result::Pause Aktualizr::Pause() {
  return api_queue_.pause(true) ? result::PauseStatus::kSuccess : result::PauseStatus::kAlreadyPaused;
}

result::Pause Aktualizr::Resume() {
  return api_queue_.pause(false) ? result::PauseStatus::kSuccess : result::PauseStatus::kAlreadyRunning;
}

void Aktualizr::Abort() { api_queue_.abort(); }

boost::signals2::connection Aktualizr::SetSignalHandler(std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return sig_->connect(handler);
}

std::unique_ptr<StorageTargetRHandle> Aktualizr::GetStoredTarget(const Uptane::Target &target) {
  try {
    auto handle = storage_->openTargetFile(target);
    if (handle->isPartial()) {
      return std::unique_ptr<StorageTargetRHandle>();
    }
    return storage_->openTargetFile(target);
  } catch (...) {
    return std::unique_ptr<StorageTargetRHandle>();
  }
}
