#include "campaign/campaign.h"

namespace campaign {

Campaign Campaign::fromJson(const Json::Value &json) {
  try {
    if (!json.isObject()) {
      throw CampaignParseError();
    }
    std::string name = json["name"].asString();

    if (name.empty()) {
      throw CampaignParseError();
    }

    // let's allow `install_message` to be empty
    std::string install_message;
    for (const auto &o : json["metadata"]) {
      if (!o.isObject()) {
        continue;
      }
      if (o["type"] == "install") {
        if (!install_message.empty()) {
          LOG_ERROR << "Only one install message allowed";
          throw CampaignParseError();
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

  Json::Value campaigns_array;
  try {
    if (!json.isObject()) {
      throw CampaignParseError();
    }
    campaigns_array = json["campaigns"];
    if (!campaigns_array.isArray()) {
      throw CampaignParseError();
    }
  } catch (...) {
    LOG_ERROR << "Invalid campaigns object: " << json;
    return {};
  }

  for (const auto &c : campaigns_array) {
    try {
      campaigns.push_back(Campaign::fromJson(c));
    } catch (const CampaignParseError &exc) {
      LOG_ERROR << "Error parsing " << c << ": " << exc.what();
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

  LOG_TRACE << "Campaign: " << json;

  return campaignsFromJson(json);
}
}  // namespace campaign
