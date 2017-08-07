#include "uptane/uptanerepository.h"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/make_shared.hpp>

#include <stdio.h>

#include "bootstrap.h"
#include "crypto.h"
#include "invstorage.h"
#include "logger.h"
#include "openssl_compat.h"
#include "ostree.h"
#include "utils.h"

namespace Uptane {

Repository::Repository(const Config &config_in, INvStorage &storage_in)
    : config(config_in),
      director("director", config.uptane.director_server, config, storage_in),
      image("repo", config.uptane.repo_server, config, storage_in),
      storage(storage_in),
      http(),
      manifests(Json::arrayValue),
      transport(&secondaries) {
  std::vector<Uptane::SecondaryConfig>::iterator it;
  for (it = config.uptane.secondaries.begin(); it != config.uptane.secondaries.end(); ++it) {
    Secondary s(*it, this);
    addSecondary(it->ecu_serial, it->ecu_hardware_id);
    secondaries.push_back(s);
  }
}

void Repository::updateRoot(Version version) {
  director.updateRoot(version);
  image.updateRoot(version);
}

bool Repository::putManifest() { return putManifest(Json::nullValue); }

bool Repository::putManifest(const Json::Value &custom) {
  Json::Value version_manifest;
  version_manifest["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  version_manifest["ecu_version_manifest"] = transport.getManifests();

  Json::Value unsigned_ecu_version =
      OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).toEcuVersion(custom);
  Json::Value ecu_version_signed =
      Crypto::signTuf(config.uptane.private_key_path, config.uptane.public_key_path, unsigned_ecu_version);
  version_manifest["ecu_version_manifest"].append(ecu_version_signed);
  Json::Value tuf_signed =
      Crypto::signTuf(config.uptane.private_key_path, config.uptane.public_key_path, version_manifest);
  HttpResponse reponse = http.put(config.uptane.director_server + "/manifest", tuf_signed);
  return reponse.isOk();
}

// Check for consistency, signatures are already checked
bool Repository::verifyMeta(const Uptane::MetaPack &meta) {
  // verify that director and image targets are consistent
  for (std::vector<Uptane::Target>::const_iterator it = meta.director_targets.targets.begin();
       it != meta.director_targets.targets.end(); ++it) {
    std::vector<Uptane::Target>::const_iterator image_target_it;
    image_target_it = std::find(meta.image_targets.targets.begin(), meta.image_targets.targets.end(), *it);
    if (image_target_it == meta.image_targets.targets.end()) return false;
  }
  return true;
}

bool Repository::getMeta() {
  putManifest();

  Uptane::MetaPack meta;
  meta.director_root = Uptane::Root("director", director.fetchAndCheckRole(Role::Root()));
  meta.image_root = Uptane::Root("repo", image.fetchAndCheckRole(Role::Root()));

  meta.image_timestamp = Uptane::TimestampMeta(image.fetchAndCheckRole(Role::Timestamp(), Version(), &meta.image_root));
  if (meta.image_timestamp.version > image.timestampVersion()) {
    meta.image_snapshot = Uptane::Snapshot(image.fetchAndCheckRole(Role::Snapshot(), Version(), &meta.image_root));
    meta.image_targets = Uptane::Targets(image.fetchAndCheckRole(Role::Targets(), Version(), &meta.image_root));
  } else {
    meta.image_snapshot = image.snapshot();
    meta.image_targets = image.targets();
  }

  meta.director_targets = Uptane::Targets(director.fetchAndCheckRole(Role::Targets(), Version(), &meta.director_root));

  if (meta.director_root.version() > director.rootVersion() || meta.image_root.version() > image.rootVersion() ||
      meta.director_targets.version > director.targetsVersion() ||
      meta.image_timestamp.version > image.timestampVersion()) {
    if (verifyMeta(meta)) {
      storage.storeMetadata(meta);
      image.setMeta(&meta.image_root, &meta.image_targets, &meta.image_timestamp, &meta.image_snapshot);
      director.setMeta(&meta.director_root, &meta.director_targets);
    } else {
      LOGGER_LOG(LVL_info, "Metadata consistency check failed");
      return false;
    }
  }
  return true;
}

std::pair<int, std::vector<Uptane::Target> > Repository::getTargets() {
  if (!getMeta()) return std::pair<int, std::vector<Uptane::Target> >(-1, std::vector<Uptane::Target>());

  std::vector<Uptane::Target> director_targets = director.getTargets();
  int version = director.targetsVersion();
  std::vector<Uptane::Target> primary_targets;
  std::vector<Uptane::Target> secondary_targets;

  if (!director_targets.empty()) {
    for (std::vector<Uptane::Target>::iterator it = director_targets.begin(); it != director_targets.end(); ++it) {
      // TODO: support downloading encrypted targets from director
      image.saveTarget(*it);
      if (it->ecu_identifier() == config.uptane.primary_ecu_serial) {
        primary_targets.push_back(*it);
      } else {
        secondary_targets.push_back(*it);
      }
    }
    transport.sendTargets(secondary_targets);
  }
  return std::pair<uint32_t, std::vector<Uptane::Target> >(version, primary_targets);
}

bool Repository::deviceRegister() {
  std::string pkey;
  std::string cert;
  std::string ca;

  if (storage.loadTlsCreds(&ca, &cert, &pkey)) {
    LOGGER_LOG(LVL_trace, "Device already registered, proceeding");
    // set provisioned credentials
    http.setCerts(ca, cert, pkey);
    director.setTlsCreds(ca, cert, pkey);
    image.setTlsCreds(ca, cert, pkey);
    return true;
  }

  if (!storage.loadBootstrapTlsCreds(&ca, &cert, &pkey)) {
    Bootstrap boot(config.provision.provision_path);
    std::string p12_str = boot.getP12Str();
    if (p12_str.empty()) {
      return false;
    }
    FILE *reg_p12 = fmemopen(const_cast<char *>(p12_str.c_str()), p12_str.size(), "rb");

    if (!reg_p12) {
      LOGGER_LOG(LVL_error, "Could not open bootstrapped credentials");
      return false;
    }

    if (!Crypto::parseP12(reg_p12, config.provision.p12_password, &pkey, &cert, &ca)) {
      fclose(reg_p12);
      return false;
    }
    fclose(reg_p12);
    storage.storeBootstrapTlsCreds(ca, cert, pkey);
  }

  // set bootstrap credentials
  http.setCerts(ca, cert, pkey);

  Json::Value data;
  data["deviceId"] = config.uptane.device_id;
  data["ttl"] = config.provision.expiry_days;
  HttpResponse response = http.post(config.tls.server + "/devices", data);
  if (!response.isOk()) {
    LOGGER_LOG(LVL_error, "error tls registering device, response: " << response.body);
    return false;
  }

  FILE *device_p12 = fmemopen(const_cast<char *>(response.body.c_str()), response.body.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", &pkey, &cert, &ca)) {
    return false;
  }
  fclose(device_p12);
  storage.storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  http.setCerts(ca, cert, pkey);
  director.setTlsCreds(ca, cert, pkey);
  image.setTlsCreds(ca, cert, pkey);

  // TODO: acknowledgement to the server
  LOGGER_LOG(LVL_info, "Provisioned successfully on Device Gateway");
  return true;
}

bool Repository::ecuRegister() {
  // register primary ECU
  std::string public_key;
  std::string private_key;
  if (!storage.loadEcuKeys(true, config.uptane.primary_ecu_serial, config.uptane.primary_ecu_hardware_id, &public_key,
                           &private_key)) {
    if (!Crypto::generateRSAKeyPair(&public_key, &private_key)) {
      LOGGER_LOG(LVL_error, "Could not generate rsa keys for primary");
      return false;
    }
    storage.storeEcu(true, config.uptane.primary_ecu_serial, config.uptane.primary_ecu_hardware_id, public_key,
                     private_key);
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = config.uptane.primary_ecu_serial;
  all_ecus["ecus"] = Json::Value(Json::arrayValue);

  PublicKey primary_pub_key(config.uptane.public_key_path);
  Json::Value primary_ecu;
  primary_ecu["hardware_identifier"] = config.uptane.primary_ecu_hardware_id;
  primary_ecu["ecu_serial"] = config.uptane.primary_ecu_serial;
  primary_ecu["clientKey"]["keytype"] = "RSA";
  primary_ecu["clientKey"]["keyval"]["public"] = public_key;
  all_ecus["ecus"].append(primary_ecu);

  // register secondary ECUs
  std::vector<SecondaryConfig>::iterator it;
  for (it = registered_secondaries.begin(); it != registered_secondaries.end(); ++it) {
    std::string secondary_public_key;
    std::string secondary_private_key;
    if (!storage.loadEcuKeys(false, it->ecu_serial, it->ecu_hardware_id, &secondary_public_key,
                             &secondary_private_key)) {
      if (!Crypto::generateRSAKeyPair(&secondary_public_key, &secondary_private_key)) {
        LOGGER_LOG(LVL_error, "Could not generate rsa keys for secondary " << it->ecu_hardware_id << "@"
                                                                           << it->ecu_serial);
        return false;
      }
      storage.storeEcu(true, it->ecu_serial, it->ecu_hardware_id, secondary_public_key, secondary_private_key);
    }

    transport.sendPrivateKey(it->ecu_serial, secondary_private_key);
    Json::Value ecu;
    ecu["hardware_identifier"] = it->ecu_hardware_id;
    ecu["ecu_serial"] = it->ecu_serial;
    ecu["clientKey"]["keytype"] = "RSA";
    ecu["clientKey"]["keyval"]["public"] = secondary_public_key;
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http.post(config.tls.server + "/director/ecus", all_ecus);
  if (response.isOk()) {
    LOGGER_LOG(LVL_info, "Provisioned successfully on Director");
  } else {
    Json::Value resp_code = response.getJson()["code"];
    // This doesn't work device_already_registered and ecu_already_registered are possible
    if (resp_code.isString() &&
        (resp_code.asString() == "ecu_already_registered" || resp_code.asString() == "device_already_registered")) {
      LOGGER_LOG(LVL_trace, "ECUs are already registered, proceeding");
      return true;
    } else {
      LOGGER_LOG(LVL_error, "Error registering device on Uptane, response: " << response.body);
      return false;
    }
  }
  return true;
}

void Repository::addSecondary(const std::string &ecu_serial, const std::string &hardware_identifier) {
  SecondaryConfig c;
  c.ecu_serial = ecu_serial;
  c.ecu_hardware_id = hardware_identifier;
  registered_secondaries.push_back(c);
}
}
