#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <boost/filesystem.hpp>
#include <map>
#include <string>

#include <json/json.h>
#include "crypto.h"
#include "uptane/exceptions.h"

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleThreshold;

class TufRepository {
 public:
  TufRepository(const boost::filesystem::path& path_in);
  void verifyRole(const Json::Value&);
  void initRoot(const Json::Value& content);

  // all of the update* methods throw uptane::SecurityException if the signatures are incorrect
  void updateRoot(const Json::Value& new_contents);
  bool updateTimestamp(const Json::Value& new_contents);
  void updateSnapshot(const Json::Value& new_contents);
  void updateTargets(const Json::Value& new_contents);

 private:
  static const int kMinSignatures = 1;
  static const int kMaxSignatures = 1000;

  void saveRole(const Json::Value& content);

  boost::filesystem::path path_;
  std::map<std::string, PublicKey> keys_;
  RoleThreshold thresholds_;
  Json::Value timestamp_signed_;

  // TODO: list of targets
};
}

#endif