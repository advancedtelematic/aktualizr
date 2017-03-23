#include "uptane.h"

#include <boost/algorithm/string/replace.hpp>
#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "json/json.h"
#include "logger.h"

Uptane::Uptane(const Config &config_in) : config(config_in) {
  http = new HttpClient();
  http->authenticate(config.device.certificates_path + config.tls.client_certificate,
                     config.device.certificates_path + config.tls.ca_file);

  initService(Director);
  initService(Repo);
}

void Uptane::initService(Uptane::ServiceType service) {
  Json::Value root_json = getJSON(service, "root");
  if (!root_json) {
    LOGGER_LOG(LVL_debug, "JSON is empty");
    return;
  }
  Json::Value json_keys = root_json["signed"]["keys"];
  for (Json::ValueIterator it = json_keys.begin(); it != json_keys.end(); it++) {
    services[service].keys[it.key().asString()] = (*it)["keyval"]["public"].asString();
  }

  Json::Value json_roles = root_json["signed"]["roles"];
  for (Json::ValueIterator it = json_roles.begin(); it != json_roles.end(); it++) {
    services[service].roles[it.key().asString()].first = (*it)["threshold"].asUInt();
  }
  verifyData(service, "root", root_json);
}

Json::Value Uptane::getJSON(Uptane::ServiceType service, const std::string &role) {
  boost::filesystem::path path = config.uptane.metadata_path;
  if (service == Director) {
    path /= "director";
  } else {
    path /= "repo";
  }
  path /= role + ".json";

  if (boost::filesystem::exists(path)) {
    std::ifstream path_stream(path.c_str());
    std::string json_content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
    Json::Value parsed_json;
    Json::Reader().parse(json_content, parsed_json);
    return parsed_json;
  } else {
    return http->getJson(getEndPointUrl(service, role));
  }
}

bool Uptane::verify(Uptane::ServiceType service, const std::string &role, Uptane::Verified &verified) {
  Json::Value data = getJSON(service, role);
  verified.old_version = services[service].roles[role].second;
  if (!verifyData(service, role, data)) {
    return false;
  }
  verified.new_version = services[service].roles[role].second;
  verified.role = role;
  verified.data = data["sigend"];
  return true;
}

bool Uptane::verifyData(Uptane::ServiceType service, const std::string &role, const Json::Value &tuf_signed) {
  if (!tuf_signed["signatures"].size()) {
    LOGGER_LOG(LVL_debug, "Missing signatures, verification failed");
    return false;
  }
  std::string canonical = Json::FastWriter().write(tuf_signed["signed"]);
  unsigned int valid_signatures = 0;
  for (Json::ValueIterator it = tuf_signed["signatures"].begin(); it != tuf_signed["signatures"].end(); it++) {
    if ((*it)["method"].asString() != "RSASSA-PSS") {
      LOGGER_LOG(LVL_debug, "Unknown sign method: " << (*it)["method"].asString());
      continue;
    }
    std::string keyid = (*it)["keyid"].asString();
    if (!services[service].keys.count(keyid)) {
      LOGGER_LOG(LVL_debug, "Couldn't find a key: " << keyid);
      continue;
    }
    std::string sig = boost::algorithm::unhex((*it)["sig"].asString());

    // FIXME Hack, because of python signs interpreted new line but returns escaped
    canonical = boost::replace_all_copy(canonical, "\\n", "\n");
    valid_signatures += Crypto::RSAPSSVerify(services[service].keys[keyid], sig, canonical);
  }
  if (valid_signatures == 0) {
    LOGGER_LOG(LVL_debug, "signature verification failed");
  } else if (valid_signatures < services[service].roles[role].first) {
    LOGGER_LOG(LVL_debug, "signature treshold error");
  } else {
    services[service].roles[role].second = tuf_signed["signed"]["version"].asUInt();
    return true;
  }
  return false;
}

std::string Uptane::getEndPointUrl(Uptane::ServiceType service, const std::string &role) {
  if (service == Director) {
    return config.uptane.director_server + "/" + role + ".json";
  } else {
    return config.uptane.repo_server + "/" + config.device.uuid + "/" + role + ".json";
  }
}