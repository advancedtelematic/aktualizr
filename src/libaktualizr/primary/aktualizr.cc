#include "aktualizr.h"

#include <chrono>

#include "utilities/timer.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

using std::make_shared;
using std::shared_ptr;

Aktualizr::Aktualizr(Config &config) : config_(config) {
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

  sig_ = make_shared<boost::signals2::signal<void(shared_ptr<event::BaseEvent>)>>();
  storage_ = INvStorage::newStorage(config_.storage);
  storage_->importData(config_.import);
  uptane_client_ = SotaUptaneClient::newDefaultClient(config_, storage_, sig_);
  uptane_client_->initialize();
}

int Aktualizr::Run() {
  SendDeviceData();
  while (!shutdown_) {
    FetchMetadata();
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

void Aktualizr::Download(std::vector<Uptane::Target> updates) { uptane_client_->downloadImages(std::move(updates)); }

void Aktualizr::Install(std::vector<Uptane::Target> updates) { uptane_client_->uptaneInstall(std::move(updates)); }

boost::signals2::connection Aktualizr::SetSignalHandler(std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return sig_->connect(handler);
}
