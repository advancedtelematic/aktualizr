#include "aktualizr.h"

#include "utilities/timer.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

#include "eventsinterpreter.h"
#include "sotauptaneclient.h"
#include "storage/invstorage.h"

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

  commands_channel_ = std::make_shared<command::Channel>();
  events_channel_ = std::make_shared<event::Channel>();
}

int Aktualizr::run() {
  EventsInterpreter events_interpreter(config_, events_channel_, commands_channel_);

  // run events interpreter in background
  events_interpreter.interpret();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config_.storage);
  storage->importData(config_.import);

  std::shared_ptr<SotaUptaneClient> uptane_client =
      SotaUptaneClient::newDefaultClient(config_, storage, events_channel_);
  uptane_client->runForever(commands_channel_);

  return EXIT_SUCCESS;
}

void Aktualizr::sendCommand(std::shared_ptr<command::BaseCommand> command) { *commands_channel_ << command; }
