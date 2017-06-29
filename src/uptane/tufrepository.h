#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <boost/filesystem.hpp>

#include <map>
#include <string>

#include <json/json.h>
#include "config.h"
#include "crypto.h"
#include "httpclient.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleThreshold;

class TufRepository {
 public:
  TufRepository(const std::string &name, const std::string &base_url, const Config &config);
  void verifyRole(Role role, const Json::Value &);

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  void updateRoot(Version version);
  bool checkTimestamp();

  Json::Value getJSON(const std::string &role);
  Json::Value fetchAndCheckRole(Role role, Version fetch_version = Version());
  std::vector<Target> getTargets() { return targets_; }

  void refresh();

 private:
  std::string downloadTarget(Target target);
  void saveTarget(const Target &target);

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