#include "uptane/tufrepository.h"

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sstream>

#include <algorithm>
#include <fstream>

#include "crypto.h"
#include "logger.h"
#include "utils.h"

namespace Uptane {

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  Uptane::DownloadMetaStruct* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  ds->downloaded_length += size * nmemb;
  if (ds->downloaded_length > ds->expected_length) {
    return (size * nmemb) + 1;  // curl will abort if return unexpected size;
  }
  fwrite(contents, size, nmemb, ds->fp);
  size_t data_size = size * nmemb;
  ds->sha256_hasher.update((const unsigned char*)contents, data_size);
  ds->sha512_hasher.update((const unsigned char*)contents, data_size);
  return data_size;
}

TufRepository::TufRepository(const std::string& name, const std::string& base_url, const Config& config)
    : root_(Root::kRejectAll),
      name_(name),
      path_(config.uptane.metadata_path / name_),
      config_(config),
      last_timestamp_(0),
      base_url_(base_url) {
  boost::filesystem::create_directories(path_);
  boost::filesystem::create_directories(path_ / "targets");
  http_.authenticate((config_.device.certificates_directory / config_.tls.client_certificate).string(),
                     (config_.device.certificates_directory / config_.tls.ca_file).string(),
                     (config_.device.certificates_directory / config_.tls.pkey_file).string());
  LOGGER_LOG(LVL_debug, "TufRepository looking for root.json in:" << path_);
  if (boost::filesystem::exists(path_ / "root.json")) {
    // Bootstrap root.json (security assumption: the filesystem contents are not modified)
    Json::Value root_json = Utils::parseJSONFile(path_ / "root.json");
    root_ = Root(name_, root_json["signed"]);
  } else {
    LOGGER_LOG(LVL_info, path_ << " not found, will provision from server");
    root_ = Root(Root::kAcceptAll);
  }
  if (boost::filesystem::exists(path_ / "timestamp.json")) {
    Json::Value timestamp_json = Utils::parseJSONFile(path_ / "timestamp.json");
    last_timestamp_ = timestamp_json["signed"]["version"].asInt();
    LOGGER_LOG(LVL_debug, "Read last timestamp.json version:" << last_timestamp_ << " for " << name);
  }
}

Json::Value TufRepository::getJSON(const std::string& role) {
  HttpResponse response = http_.get(base_url_ + "/" + role);
  if (!response.isOk()) {
    throw MissingRepo(name_);
  }
  return response.getJson();
}

void TufRepository::updateRoot(Version version) {
  Json::Value new_root_json = fetchAndCheckRole(Role::Root(), version);
  // Validation passed.
  root_ = Root(name_, new_root_json);
}

bool TufRepository::checkTimestamp() {
  Json::Value content = fetchAndCheckRole(Role::Timestamp(), Version());
  int new_timestamp = content["version"].asInt();
  bool has_changed = last_timestamp_ < new_timestamp;
  last_timestamp_ = new_timestamp;
  return has_changed;
}

/**
 * Note this doesn't check the validity of version numbers, only that the signed part is actually signed.
 * @param role
 * @return The contents of the "signed" section
 */
Json::Value TufRepository::fetchAndCheckRole(Uptane::Role role, Version version) {
  TimeStamp now(TimeStamp::Now());
  Json::Value content = getJSON(version.RoleFileName(role));
  return verifyRole(role, now, content);
}

Json::Value TufRepository::verifyRole(Uptane::Role role, const TimeStamp& now, const Json::Value& content) {
  Json::Value result = root_.UnpackSignedObject(now, name_, role, content);
  if (role == Role::Root()) {
    // Also check that the new root is suitably self-signed
    Root new_root = Root(name_, result);
    new_root.UnpackSignedObject(now, name_, Role::Root(), content);
  }
  // UnpackSignedObject throws on error--write the (now known to be valid) content to disk
  Utils::writeFile((path_ / (role.ToString() + ".json")).string(), content);
  return result;
}

std::string TufRepository::downloadTarget(Target target) {
  DownloadMetaStruct ds;
  FILE* fp = fopen((path_ / "targets" / target.filename()).string().c_str(), "w");
  ds.fp = fp;
  ds.downloaded_length = 0;
  ds.expected_length = target.length();

  HttpResponse response = http_.download(base_url_ + "/targets/" + target.filename(), DownloadHandler, &ds);
  fclose(fp);
  if (!response.isOk()) {
    if (response.curl_code == CURLE_WRITE_ERROR) {
      throw OversizedTarget(target.filename());
    } else {
      throw Exception(name_, "Could not download file, error: " + response.error_message);
    }
  }
  std::string h256 = ds.sha256_hasher.getHexDigest();
  std::string h512 = ds.sha512_hasher.getHexDigest();

  if (!target.MatchWith(Hash(Hash::kSha256, h256)) && !target.MatchWith(Hash(Hash::kSha512, h512))) {
    throw TargetHashMismatch(target.filename());
  }
  return response.body;
}

void TufRepository::saveTarget(const Target& target) {
  if (target.length() > 0) {
    std::string content = downloadTarget(target);
    Utils::writeFile((path_ / "targets" / target.filename()).string(), content);
  }
}

std::pair<uint32_t, std::vector<Target>> TufRepository::fetchTargets() {
  targets_.clear();  // TODO, this is used to signal 'no new updates'
  Json::Value targets_json = fetchAndCheckRole(Role::Targets());
  Json::Value target_list = targets_json["targets"];
  for (Json::ValueIterator t_it = target_list.begin(); t_it != target_list.end(); t_it++) {
    Target t(t_it.key().asString(), *t_it);
    targets_.push_back(t);
  }
  return std::pair<uint32_t, std::vector<Target>>(targets_json["version"].asInt(), targets_);
}
};
