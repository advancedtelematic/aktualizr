#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "crypto.h"
#include "packagemanagerinterface.h"

static std::string refhash_fake;

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake() {
    // Default to a dummy hash so that we can send something to the server and
    // tests have something to check for.
    refhash_fake = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("0")));
  }
  Json::Value getInstalledPackages() override {
    Json::Value packages(Json::arrayValue);
    Json::Value package;
    package["name"] = "fake-package";
    package["version"] = "1.0";
    packages.append(package);
    return packages;
  }

  Uptane::Target getCurrent() override {
    Json::Value t_json;
    t_json["hashes"]["sha256"] = refhash_fake;
    t_json["length"] = 0;
    return Uptane::Target(std::string("unknown-") + refhash_fake, t_json);
  }

  data::InstallOutcome install(const Uptane::Target &target) const override {
    (void)target;
    refhash_fake = target.sha256Hash();
    return data::InstallOutcome(data::OK, "Pulling ostree image was successful");
  };
};

#endif  // PACKAGEMANAGERFAKE_H_
