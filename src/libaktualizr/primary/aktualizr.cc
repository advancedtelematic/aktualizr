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
  api_thread_ = std::thread([&]() {
    while (!shutdown_) {
      auto task = api_queue_.dequeue();
      if (shutdown_) {
        return;
      }
      task();
    }
  });
}

void Aktualizr::UptaneCycle() {
  RunningMode running_mode = config_.uptane.running_mode;
  result::UpdateCheck update_result = CheckUpdates().get();
  if (running_mode == RunningMode::kCheck || update_result.updates.size() == 0) {
    return;
  } else if (running_mode == RunningMode::kInstall) {
    uptane_client_->uptaneInstall(update_result.updates);
    uptane_client_->putManifest();
    return;
  }

  result::Download download_result = Download(update_result.updates).get();
  if (running_mode == RunningMode::kDownload || download_result.status != result::DownloadStatus::kSuccess ||
      download_result.updates.size() == 0) {
    return;
  }

  uptane_client_->uptaneInstall(download_result.updates);
  uptane_client_->putManifest();
}

std::future<void> Aktualizr::RunForever() {
  std::future<void> future = std::async(std::launch::async, [&]() {
    SendDeviceData().get();
    while (!shutdown_) {
      UptaneCycle();
      std::this_thread::sleep_for(std::chrono::seconds(config_.uptane.polling_sec));
    }
  });
  return future;
}

void Aktualizr::AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &secondary) {
  uptane_client_->addNewSecondary(secondary);
}

void Aktualizr::Shutdown() {
  if (!shutdown_) {
    shutdown_ = true;
    api_queue_.shutDown();
  }
}

std::future<result::CampaignCheck> Aktualizr::CampaignCheck() {
  auto promise = std::make_shared<std::promise<result::CampaignCheck>>();
  std::function<void()> task([&, promise]() { promise->set_value(uptane_client_->campaignCheck()); });
  api_queue_.enqueue(task);
  return promise->get_future();
}

std::future<void> Aktualizr::CampaignAccept(const std::string &campaign_id) {
  auto promise = std::make_shared<std::promise<void>>();
  std::function<void()> task([&, promise, campaign_id]() {
    uptane_client_->campaignAccept(campaign_id);
    promise->set_value();
  });
  api_queue_.enqueue(task);
  return promise->get_future();
}

std::future<void> Aktualizr::SendDeviceData() {
  auto promise = std::make_shared<std::promise<void>>();
  std::function<void()> task([&, promise]() {
    uptane_client_->sendDeviceData();
    promise->set_value();
  });
  api_queue_.enqueue(task);
  return promise->get_future();
}

std::future<result::UpdateCheck> Aktualizr::CheckUpdates() {
  auto promise = std::make_shared<std::promise<result::UpdateCheck>>();
  std::function<void()> task([&, promise]() { promise->set_value(uptane_client_->fetchMeta()); });
  api_queue_.enqueue(task);
  return promise->get_future();
}

std::future<result::Download> Aktualizr::Download(const std::vector<Uptane::Target> &updates) {
  auto promise = std::make_shared<std::promise<result::Download>>();
  std::function<void()> task(
      [this, promise, updates]() { promise->set_value(uptane_client_->downloadImages(updates)); });
  api_queue_.enqueue(task);
  return promise->get_future();
}

std::future<result::Install> Aktualizr::Install(const std::vector<Uptane::Target> &updates) {
  auto promise = std::make_shared<std::promise<result::Install>>();
  std::function<void()> task(
      [this, promise, updates]() { promise->set_value(uptane_client_->uptaneInstall(updates)); });
  api_queue_.enqueue(task);
  return promise->get_future();
}

result::Pause Aktualizr::Pause() { return uptane_client_->pause(); }

result::Pause Aktualizr::Resume() { return uptane_client_->resume(); }

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
