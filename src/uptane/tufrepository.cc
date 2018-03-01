#include "uptane/tufrepository.h"

#include <fcntl.h>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "keymanager.h"
#include "logging.h"
#include "utils.h"

namespace Uptane {

TufRepository::TufRepository(const std::string& name, const std::string& base_url, const Config& config,
                             boost::shared_ptr<INvStorage>& storage)
    : name_(name), config_(config), storage_(storage), base_url_(base_url), root_(Root::kRejectAll) {
  Uptane::MetaPack meta;
  if (storage_->loadMetadata(&meta)) {
    if (name_ == "repo") {
      root_ = meta.image_root;
      targets_ = meta.image_targets;
      timestamp_ = meta.image_timestamp;
      snapshot_ = meta.image_snapshot;
    } else if (name == "director") {
      root_ = meta.director_root;
      targets_ = meta.director_targets;
    }
  } else {
    root_ = Root(Root::kAcceptAll);
  }
}

Json::Value TufRepository::verifyRole(Uptane::Role role, const Json::Value& content, Uptane::Root* root_used) const {
  TimeStamp now(TimeStamp::Now());
  if (!root_used) root_used = const_cast<Uptane::Root*>(&root_);
  Json::Value result = root_used->UnpackSignedObject(now, name_, role, content);
  if (role == Role::Root()) {
    // Also check that the new root is suitably self-signed
    Root new_root = Root(name_, result);
    new_root.UnpackSignedObject(now, name_, Role::Root(), content);
  }
  return result;
}

void TufRepository::setMeta(Uptane::Root* root, Uptane::Targets* targets, Uptane::TimestampMeta* timestamp,
                            Uptane::Snapshot* snapshot) {
  if (root) root_ = *root;
  if (targets) targets_ = *targets;
  if (timestamp) timestamp_ = *timestamp;
  if (snapshot) snapshot_ = *snapshot;
}
}  // namespace Uptane
