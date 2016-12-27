#include "sotahttpclient.h"

#include <boost/property_tree/json_parser.hpp>
#include "time.h"

#include "logger.h"

SotaHttpClient::SotaHttpClient(const Config &config_in) : config(config_in) {
  http.authenticate(config.auth);
  core_url = config.core.server + "/api/v1";
}

boost::property_tree::ptree SotaHttpClient::getAvailableUpdates() {
  std::string url =
      core_url + "/device_updates/" + config.device.uuid + "/queued";
  return http.get(url);
}

boost::property_tree::ptree SotaHttpClient::downloadUpdate(
    const boost::property_tree::ptree &update) {
  std::string url = core_url + "/device_updates/" + config.device.uuid + "/" +
                    update.get<std::string>("requestId") + "/download";
  std::string filename =
      config.device.packages_dir + update.get<std::string>("packageId.name");
  http.download(url, filename);
  boost::property_tree::ptree status;
  status.put("updateId", update.get<std::string>("requestId"));
  status.put("resultCode", 0);
  status.put("resultText", "Downloaded");

  time_t _tm = time(NULL);
  tm *curtime = localtime(&_tm);
  status.put("receivedAt", asctime(curtime));
  return status;
}

boost::property_tree::ptree SotaHttpClient::reportUpdateResult(
    const boost::property_tree::ptree &download_status) {
  std::string url = core_url + "/device_updates/" + config.device.uuid;
  std::stringstream ss;
  ss << '[';
  boost::property_tree::json_parser::write_json(ss, download_status);
  ss << ']';
  return http.post(url, ss.str());
}
