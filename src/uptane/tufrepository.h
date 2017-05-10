#ifndef UPTANE_TUFREPOSITORY_H_
#define UPTANE_TUFREPOSITORY_H_

#include <json/json.h>
#include <boost/filesystem.hpp>
#include "uptane/exceptions.h"

#include <map>
#include <string>

namespace Uptane {
typedef std::map<std::string, unsigned int> RoleTreshold;
struct PublicKey {
  PublicKey() {}
  PublicKey(const std::string& v, const std::string& t) : value(v), type(t) {}
  std::string value;
  std::string type;
};

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
  void saveRole(const Json::Value& content);
  boost::filesystem::path path;
  std::map<std::string, PublicKey> keys;
  RoleTreshold tresholds;
  Json::Value timestamp_signed;

  // TODO: list of targets
};
}

#endif