#ifndef PACKAGEMANAGERINTERFACE_H_
#define PACKAGEMANAGERINTERFACE_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "types.h"
#include "uptane/tuf.h"

class PackageManagerInterface {
 public:
  virtual ~PackageManagerInterface() = default;
  virtual Json::Value getInstalledPackages() = 0;
  virtual Uptane::Target getCurrent() = 0;
  virtual data::InstallOutcome install(const Uptane::Target &target) const = 0;
  Uptane::Target getUnknown() {
    Json::Value t_json;
    t_json["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
    t_json["length"] = 0;
    return Uptane::Target("unknown", t_json);
  }
};

#endif  // PACKAGEMANAGERINTERFACE_H_
