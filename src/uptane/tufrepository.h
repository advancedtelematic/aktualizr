#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <boost/filesystem.hpp>

#include <map>
#include <string>
#include <utility>

#include <json/json.h>
#include "config.h"
#include "crypto.h"
#include "httpclient.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleThreshold;

struct DownloadMetaStruct {
  int64_t expected_length;
  int64_t downloaded_length;
  FILE *fp;
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

class TufRepository {
 public:
  TufRepository(const std::string &name, const std::string &base_url, const Config &config);
  Json::Value verifyRole(Role role, const TimeStamp &now, const Json::Value &);

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  void updateRoot(Version version);
  bool checkTimestamp();

  Json::Value getJSON(const std::string &role);
  Json::Value fetchAndCheckRole(Role role, Version fetch_version = Version());
  std::vector<Target> getTargets() { return targets_; }
  std::pair<uint32_t, std::vector<Target>> fetchTargets();
  void saveTarget(const Target &target);
  std::string downloadTarget(Target target);

 private:
  std::string name_;
  boost::filesystem::path path_;
  Config config_;
  Uptane::Root root_;
  int last_timestamp_;
  HttpClient http_;
  std::vector<Target> targets_;
  std::string base_url_;
  friend class TestBusSecondary;

  // TODO: list of targets
};
}

#endif
