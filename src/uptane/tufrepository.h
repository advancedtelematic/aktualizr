#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <boost/filesystem.hpp>

#include <map>
#include <string>
#include <utility>

#include <json/json.h>
#include "config.h"
#include "crypto.h"
#include "httpinterface.h"
#include "invstorage.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleThreshold;

struct DownloadMetaStruct {
  int64_t expected_length;
  int64_t downloaded_length;
  StorageTargetWHandle* fhandle;
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

class TufRepository {
 public:
  TufRepository(const std::string& name, const std::string& base_url, const Config& config,
                boost::shared_ptr<INvStorage>& storage, HttpInterface& http_client);
  Json::Value verifyRole(Role role, const TimeStamp& now, const Json::Value&, Uptane::Root* root_used = NULL);

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  void updateRoot(Version version);
  bool checkTimestamp();

  Json::Value getJSON(const std::string& role);
  Json::Value fetchAndCheckRole(Role role, Version version = Version(), Uptane::Root* root_used = NULL);
  std::vector<Target> getTargets() { return targets_.targets; }
  // std::pair<uint32_t, std::vector<Target> > fetchTargets();
  void saveTarget(const Target& target);
  std::string downloadTarget(Target target);
  int rootVersion() { return root_.version(); }
  int targetsVersion() { return targets_.version; }
  int timestampVersion() { return timestamp_.version; }

  const Uptane::Root& root() { return root_; }
  const Uptane::Targets& targets() { return targets_; }
  const Uptane::TimestampMeta& timestamp() { return timestamp_; }
  const Uptane::Snapshot& snapshot() { return snapshot_; }
  void setMeta(Uptane::Root* root, Uptane::Targets* targets, Uptane::TimestampMeta* timestamp = NULL,
               Uptane::Snapshot* snapshot = NULL);

 private:
  std::string name_;
  const Config& config_;
  boost::shared_ptr<INvStorage> storage_;
  HttpInterface& http_;
  std::string base_url_;

  // Metadata
  Uptane::Root root_;
  Uptane::Targets targets_;
  Uptane::TimestampMeta timestamp_;
  Uptane::Snapshot snapshot_;
};
}  // namespace Uptane

#endif
