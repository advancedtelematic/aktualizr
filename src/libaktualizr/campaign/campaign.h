#ifndef CAMPAIGN_CAMPAIGN_H_
#define CAMPAIGN_CAMPAIGN_H_

#include <string>
#include <vector>
#include "http/httpclient.h"
#include "utilities/utils.h"

namespace campaign {

constexpr int64_t kMaxCampaignsMetaSize = 1024 * 1024;

class CampaignParseError : std::exception {
 public:
  const char *what() const noexcept override { return "Could not parse Campaign metadata"; }
};

// Out of uptane concept: update campaign for a device
class Campaign {
 public:
  static Campaign fromJson(const Json::Value &json);

  std::string id;
  std::string name;
  bool autoAccept;
  std::string description;
  int estInstallationDuration;
  int estPreparationDuration;
};

std::vector<Campaign> campaignsFromJson(const Json::Value &json);
std::vector<Campaign> fetchAvailableCampaigns(HttpInterface &http_client, const std::string &tls_server);
}  // namespace campaign

#endif
