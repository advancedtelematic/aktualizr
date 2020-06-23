#include <libaktualizr/campaign.h>

#include "logging/logging.h"
#include "http/httpclient.h"

namespace campaign {

Campaign::Campaign(const Json::Value &json) {
  try {
    if (!json.isObject()) {
      throw CampaignParseError();
    }

    id = json["id"].asString();
    if (id.empty()) {
      throw CampaignParseError();
    }

    name = json["name"].asString();
    if (name.empty()) {
      throw CampaignParseError();
    }

    size = json.get("size", 0).asInt64();
    autoAccept = json.get("autoAccept", false).asBool();

    for (const auto &o : json["metadata"]) {
      if (!o.isObject()) {
        continue;
      }

      if (o["type"] == "DESCRIPTION") {
        if (!description.empty()) {
          throw CampaignParseError();
        }
        description = o["value"].asString();
      } else if (o["type"] == "ESTIMATED_INSTALLATION_DURATION") {
        if (estInstallationDuration != 0) {
          throw CampaignParseError();
        }
        estInstallationDuration = std::stoi(o["value"].asString());
      } else if (o["type"] == "ESTIMATED_PREPARATION_DURATION") {
        if (estPreparationDuration != 0) {
          throw CampaignParseError();
        }
        estPreparationDuration = std::stoi(o["value"].asString());
      }
    }

  } catch (const std::runtime_error &exc) {
    LOG_ERROR << exc.what();
    throw CampaignParseError();
  } catch (...) {
    throw CampaignParseError();
  }
}

void Campaign::getJson(Json::Value &out) const {
  out.clear();

  out["id"] = id;
  out["name"] = name;
  out["size"] = Json::UInt(size);
  out["autoAccept"] = autoAccept;

  out["metadata"][0]["type"] = "DESCRIPTION";
  out["metadata"][0]["value"] = description;
  out["metadata"][1]["type"] = "ESTIMATED_INSTALLATION_DURATION";
  out["metadata"][1]["value"] = std::to_string(estInstallationDuration);
  out["metadata"][2]["type"] = "ESTIMATED_PREPARATION_DURATION";
  out["metadata"][2]["value"] = std::to_string(estPreparationDuration);
}

std::vector<Campaign> Campaign::campaignsFromJson(const Json::Value &json) {
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
      campaigns.emplace_back(Campaign(c));
    } catch (const CampaignParseError &exc) {
      LOG_ERROR << "Error parsing " << c << ": " << exc.what();
    }
  }
  return campaigns;
}

void Campaign::JsonFromCampaigns(const std::vector<Campaign> &in, Json::Value &out) {
  out.clear();
  auto i = 0;
  Json::Value json;
  for (const auto &c : in) {
    c.getJson(json);
    out["campaigns"][i] = json;
    ++i;
  }
}

std::vector<Campaign> Campaign::fetchAvailableCampaigns(HttpInterface &http_client, const std::string &tls_server) {
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
