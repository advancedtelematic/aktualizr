#ifndef AKTUALIZR_LITE_HELPERS
#define AKTUALIZR_LITE_HELPERS

#include <string>

#include <string.h>

struct Version {
  std::string raw_ver;
  Version(std::string version) : raw_ver(std::move(version)) {}

  bool operator<(const Version& other) { return strverscmp(raw_ver.c_str(), other.raw_ver.c_str()) < 0; }
};

#endif  // AKTUALIZR_LITE_HELPERS
