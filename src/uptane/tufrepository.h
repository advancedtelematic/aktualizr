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

class TufRepository {
 public:
  TufRepository(const std::string& name, const std::string& base_url, const Config& config,
                boost::shared_ptr<INvStorage>& storage);
  Json::Value verifyRole(Role role, const Json::Value&, Uptane::Root* root_used = NULL) const;
  std::string getBaseUrl() const { return base_url_; }

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  bool checkTimestamp();

  std::vector<Target> getTargets() { return targets_.targets; }
  int rootVersion() { return root_.version(); }
  int targetsVersion() { return targets_.version(); }
  int timestampVersion() { return timestamp_.version(); }

  const Uptane::Root& root() { return root_; }
  const Uptane::Targets& targets() { return targets_; }
  const Uptane::TimestampMeta& timestamp() { return timestamp_; }
  const Uptane::Snapshot& snapshot() { return snapshot_; }
  void setMeta(Uptane::Root* root, Uptane::Targets* targets = NULL, Uptane::TimestampMeta* timestamp = NULL,
               Uptane::Snapshot* snapshot = NULL);

 private:
  std::string name_;
  const Config& config_;
  boost::shared_ptr<INvStorage> storage_;
  std::string base_url_;

  // Metadata
  Uptane::Root root_;
  Uptane::Targets targets_;
  Uptane::TimestampMeta timestamp_;
  Uptane::Snapshot snapshot_;
};
}  // namespace Uptane

#endif
