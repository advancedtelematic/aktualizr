#include "campaign/campaign.h"

namespace campaign {

Campaign Campaign::fromJson(const Json::Value &json) {
  try {
    std::string name = json["name"].asString();

    if (name.empty()) {
      throw;
    }

    // let's allow `install_message` to be empty
    std::string install_message;
    for (const auto &o : json["metadata"]) {
      if (o["type"] == "install") {
        if (!install_message.empty()) {
          LOG_ERROR << "Only one install message allowed";
          throw;
        }
        install_message = o["value"].asString();
      }
    }

    return {name, install_message};
  } catch (const std::runtime_error &exc) {
    LOG_ERROR << exc.what();
    throw CampaignParseError();
  } catch (...) {
    throw CampaignParseError();
  }
}

std::vector<Campaign> campaignsFromJson(const Json::Value &json) {
  std::vector<Campaign> campaigns;
  for (const auto &o : json) {
    try {
      campaigns.push_back(Campaign::fromJson(o));
    } catch (const CampaignParseError &exc) {
      LOG_ERROR << "Error parsing " << o << ": " << exc.what();
    }
  }
  return campaigns;
}

std::vector<Campaign> fetchAvailableCampaigns(HttpInterface &http_client, const std::string &tls_server) {
  HttpResponse response = http_client.get(tls_server + "/campaigner/campaigns", kMaxCampaignsMetaSize);
  if (!response.isOk()) {
    LOG_ERROR << "Failed to fetch list of available campaigns";
    return {};
  }

  auto json = response.getJson();

  return campaignsFromJson(json);
}
}  // namespace campaign
