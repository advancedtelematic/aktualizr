#include "aktualizr.h"

#include "utilities/timer.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

#include "eventsinterpreter.h"
#include "utilities/channel.h"

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

  commands_channel_ = make_shared<command::Channel>();
  events_channel_ = make_shared<event::Channel>();
  sig_ = make_shared<boost::signals2::signal<void(shared_ptr<event::BaseEvent>)>>();
  storage_ = INvStorage::newStorage(config_.storage);
  storage_->importData(config_.import);
  uptane_client_ = SotaUptaneClient::newDefaultClient(config_, storage_, events_channel_);
}

int Aktualizr::Run() {
  // run events interpreter in background
  events_interpreter_ = std::make_shared<EventsInterpreter>(config_, events_channel_, commands_channel_, sig_);
  events_interpreter_->interpret();
  uptane_client_->runForever(commands_channel_);

  return EXIT_SUCCESS;
}

void Aktualizr::AddSecondary(const std::shared_ptr<Uptane::SecondaryInterface> &secondary) {
  uptane_client_->addNewSecondary(secondary);
}

void Aktualizr::Shutdown() { *commands_channel_ << make_shared<command::Shutdown>(); }

void Aktualizr::CampaignCheck() { *commands_channel_ << make_shared<command::CampaignCheck>(); }

void Aktualizr::SendDeviceData() { *commands_channel_ << make_shared<command::SendDeviceData>(); }

void Aktualizr::FetchMetadata() { *commands_channel_ << make_shared<command::FetchMeta>(); }

void Aktualizr::CheckUpdates() { *commands_channel_ << make_shared<command::CheckUpdates>(); }

void Aktualizr::Download(std::vector<Uptane::Target> updates) {
  *commands_channel_ << make_shared<command::StartDownload>(std::move(updates));
}

void Aktualizr::Install(std::vector<Uptane::Target> updates) {
  *commands_channel_ << make_shared<command::UptaneInstall>(std::move(updates));
}

boost::signals2::connection Aktualizr::SetSignalHandler(std::function<void(shared_ptr<event::BaseEvent>)> &handler) {
  return (*sig_).connect(handler);
}
