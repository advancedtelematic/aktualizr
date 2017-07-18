#include "aktualizr.h"

#include "channel.h"
#include "commands.h"
#include "events.h"
#include "eventsinterpreter.h"
#include "sotahttpclient.h"
#ifdef WITH_GENIVI
#include "sotarviclient.h"
#endif

#ifdef BUILD_OSTREE
#include "ostree.h"
#include "sotauptaneclient.h"
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>

Aktualizr::Aktualizr(const Config &config) : config_(config) {
  if (sodium_init() == -1) {
    throw std::runtime_error("Unable to initialize libsodium");
  }

  RAND_poll();

  unsigned int seed;
  std::ifstream urandom("/dev/random", std::ios::in | std::ios::binary);
  urandom.read(reinterpret_cast<char *>(&seed), sizeof(seed));
  urandom.close();
  std::srand(seed);  // seeds pseudo random generator with random number
  
#ifdef BUILD_OSTREE
  try {
    OstreeBranch::getCurrent("", config.ostree.sysroot);
  } catch (...) {
    throw std::runtime_error("Could not load installed ostree package");
  }
#endif
}

int Aktualizr::run() {
  command::Channel *commands_channel = new command::Channel();
  event::Channel *events_channel = new event::Channel();

  EventsInterpreter events_interpreter(config_, events_channel, commands_channel);
  // run events interpreter in background
  events_interpreter.interpret();

  if (config_.gateway.rvi) {
#ifdef WITH_GENIVI
    try {
      SotaRVIClient(config_, events_channel).runForever(commands_channel);
    } catch (std::runtime_error e) {
      LOGGER_LOG(LVL_error, "Missing RVI configurations: " << e.what());
      return EXIT_FAILURE;
    }
#else
    LOGGER_LOG(LVL_error, "RVI support is not enabled");
    return EXIT_FAILURE;
#endif
  } else {
    if (config_.core.auth_type == CERTIFICATE) {
#ifdef BUILD_OSTREE

      SotaUptaneClient(config_, events_channel).runForever(commands_channel);
#endif
    } else {
      SotaHttpClient(config_, events_channel).runForever(commands_channel);
    }
  }
  return EXIT_SUCCESS;
}