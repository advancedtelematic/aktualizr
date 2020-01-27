#ifndef AKTUALIZR_LITE_HELPERS
#define AKTUALIZR_LITE_HELPERS

#include <string>

#include <string.h>

#include "primary/sotauptaneclient.h"

struct Version {
  std::string raw_ver;
  Version(std::string version) : raw_ver(std::move(version)) {}

  bool operator<(const Version& other) { return strverscmp(raw_ver.c_str(), other.raw_ver.c_str()) < 0; }
};

struct LiteClient {
  LiteClient(Config& config_in);

  Config config;
  std::shared_ptr<INvStorage> storage;
  std::shared_ptr<SotaUptaneClient> primary;
  std::pair<Uptane::EcuSerial, Uptane::HardwareIdentifier> primary_ecu;
};

#endif  // AKTUALIZR_LITE_HELPERS
