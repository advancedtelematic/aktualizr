#include "aktualizr.h"

#include "timer.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

#include "channel.h"
#include "commands.h"
#include "events.h"
#include "eventsinterpreter.h"
#include "fsstorage.h"
#include "httpclient.h"

#ifdef WITH_GENIVI
#include "sotarviclient.h"
#endif

#ifdef BUILD_OSTREE
#include "ostree.h"
#include "sotauptaneclient.h"
#endif

Aktualizr::Aktualizr(const Config &config) : config_(config) {
  if (sodium_init() == -1) {  // Note that sodium_init doesn't require a matching 'sodium_deinit'
    throw std::runtime_error("Unable to initialize libsodium");
  }

  LOGGER_LOG(LVL_trace, "Seeding random number generator from /dev/random...");
  Timer timer;
  unsigned int seed;
  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  urandom.read(reinterpret_cast<char *>(&seed), sizeof(seed));
  urandom.close();
  std::srand(seed);  // seeds pseudo random generator with random number
  LOGGER_LOG(LVL_trace, "... seeding complete in " << timer);

#ifdef BUILD_OSTREE
  try {
    OstreePackage::getCurrent(config.ostree.sysroot);
  } catch (...) {
    throw std::runtime_error("Could not find OSTree sysroot at:" + config.ostree.sysroot);
  }
#endif
}

int Aktualizr::run() {
  command::Channel commands_channel;
  event::Channel events_channel;

  EventsInterpreter events_interpreter(config_, &events_channel, &commands_channel);

  // run events interpreter in background
  events_interpreter.interpret();

  if (config_.gateway.rvi) {
#ifdef WITH_GENIVI
    try {
      SotaRVIClient(config_, &events_channel).runForever(&commands_channel);
    } catch (std::runtime_error e) {
      LOGGER_LOG(LVL_error, "Missing RVI configurations: " << e.what());
      return EXIT_FAILURE;
    }
#else
    LOGGER_LOG(LVL_error, "RVI support is not enabled");
    return EXIT_FAILURE;
#endif
  } else {
#ifdef BUILD_OSTREE
    // TODO: compile unconditionally
    FSStorage storage(config_);
    HttpClient http;
    Uptane::Repository repo(config_, storage, http);
    SotaUptaneClient(config_, &events_channel, repo).runForever(&commands_channel);
#else
    LOGGER_LOG(LVL_error, "OSTree support is disabled, but currently required for UPTANE");
    return EXIT_FAILURE;
#endif
  }
  return EXIT_SUCCESS;
}
