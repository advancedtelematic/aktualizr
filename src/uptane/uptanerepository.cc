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
#include "utils.h"

namespace Uptane {

Repository::Repository(const Config &config_in, INvStorage &storage_in, HttpInterface &http_client)
    : config(config_in),
      director("director", config.uptane.director_server, config, storage_in, http_client),
      image("repo", config.uptane.repo_server, config, storage_in, http_client),
      storage(storage_in),
      http(http_client),
#ifdef BUILD_P11
      p11(config.p11),
#endif
      manifests(Json::arrayValue) {
  // transport(&secondaries) {
}

void Repository::updateRoot(Version version) {
  director.updateRoot(version);
  image.updateRoot(version);
}

bool Repository::putManifest(const Json::Value &version_manifests) {
  Json::Value manifest;
  manifest["primary_ecu_serial"] = primary_ecu_serial;
  manifest["ecu_version_manifest"] = version_manifests;

  ENGINE *crypto_engine = NULL;
#ifdef BUILD_P11
  if (key_source == kPkcs11) crypto_engine = p11.getEngine();
#endif
  Json::Value tuf_signed = Crypto::signTuf(crypto_engine, primary_private_key, primary_public_key_id, manifest);
  HttpResponse response = http.put(config.uptane.director_server + "/manifest", tuf_signed);
  return response.isOk();
}

// array of individual ECU version manifests
Json::Value Repository::getCurrentVersionManifests(const Json::Value &primary_version_manifest) {
  Json::Value result = Json::arrayValue;
  ENGINE *crypto_engine = NULL;
#ifdef BUILD_P11
  if (key_source == kPkcs11) crypto_engine = p11.getEngine();
#endif

  Json::Value ecu_version_signed =
      Crypto::signTuf(crypto_engine, primary_private_key, primary_public_key, primary_version_manifest);
  result.append(ecu_version_signed);

  std::vector<SecondaryConfig>::iterator it;
  // for (it = config.uptane.secondary_configs.begin(); it != config.uptane.secondary_configs.end(); it++)
  // result.append(it->transport->getManifest(it->ecu_serial));
  return result;
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

  if (!director_targets.empty()) {
    for (std::vector<Uptane::Target>::iterator it = director_targets.begin(); it != director_targets.end(); ++it) {
      // TODO: support downloading encrypted targets from director
      image.saveTarget(*it);
    }
  }
  return std::pair<uint32_t, std::vector<Uptane::Target> >(version, director_targets);
}

Json::Value Repository::updateSecondaries(const std::vector<Uptane::Target> &secondary_targets) {
  // TODO: may be quite resource consuming, consider storing map ecu_serial -> SecondaryConfig as a member instead
  // map from ecu_serial -> SecondaryInterface now exists in sotauptaneclient.
  std::map<std::string, const Uptane::Target *> targets_by_serial;
  std::vector<Uptane::Target>::const_iterator t_it;
  for (t_it = secondary_targets.begin(); t_it != secondary_targets.end(); t_it++) {
    targets_by_serial[t_it->ecu_identifier()] = &*t_it;
  }
  std::vector<SecondaryConfig>::const_iterator sec_it;
  for (sec_it = config.uptane.secondary_configs.begin(); sec_it != config.uptane.secondary_configs.end(); sec_it++) {
    if (targets_by_serial.find(sec_it->ecu_serial) != targets_by_serial.end()) {
      // TODO: when metadata verification is separated from image loading (see comments in sotauptaneclient.cc) it
      // should be rewritten
      Uptane::MetaPack meta;
      if (!storage.loadMetadata(&meta)) {
        throw std::runtime_error("No valid metadata, but trying to upload firmware");
      }
      Json::Value resp;
      // if (it->partial_verifying) {
      // resp = it->transport->sendMetaPartial(meta.director_root, meta.director_targets);
      //} else {
      //}
    }
  }
  return Json::Value();  // transport.sendTargets(secondary_targets);
}

void Repository::saveInstalledVersion(const Target &target) {
  std::string versions_str;
  storage.loadInstalledVersions(&versions_str);
  Json::Value versions = Utils::parseJSON(versions_str);
  versions[boost::algorithm::to_lower_copy(target.sha256Hash())] = target.filename();
  storage.storeInstalledVersions(Json::FastWriter().write(versions));
}

std::string Repository::findInstalledVersion(const std::string &hash) {
  std::string versions_str;
  storage.loadInstalledVersions(&versions_str);
  Json::Value versions = Utils::parseJSON(versions_str);
  return versions[boost::algorithm::to_lower_copy(hash)].asString();
}
}
