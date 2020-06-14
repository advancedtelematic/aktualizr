#ifndef CAMPAIGN_CAMPAIGN_H_
#define CAMPAIGN_CAMPAIGN_H_

#include <string>
#include <vector>

#include <libaktualizr/utils.h>

#include "http/httpclient.h"

namespace campaign {

constexpr int64_t kMaxCampaignsMetaSize = 1024 * 1024;

class CampaignParseError : std::exception {
 public:
  const char *what() const noexcept override { return "Could not parse Campaign metadata"; }
};

//! @cond Doxygen_Suppress
// Annoying bug in doxygen 1.8.13
// Looks like it's fixed in later versions: https://sourceforge.net/p/doxygen/mailman/message/35481387/
enum class Cmd {
  Accept,
  Decline,
  Postpone,
};
///! @endcond

static inline Cmd cmdFromName(const std::string &name) {
  return std::map<std::string, Cmd>{
      {"campaign_accept", Cmd::Accept}, {"campaign_decline", Cmd::Decline}, {"campaign_postpone", Cmd::Postpone}}
      .at(name);
}

// Out of uptane concept: update campaign for a device
class Campaign {
 public:
  static std::vector<Campaign> campaignsFromJson(const Json::Value &json);
  static void JsonFromCampaigns(const std::vector<Campaign> &in, Json::Value &out);
  static std::vector<Campaign> fetchAvailableCampaigns(HttpInterface &http_client, const std::string &tls_server);

 public:
  Campaign() = default;
  Campaign(const Json::Value &json);
  void getJson(Json::Value &out) const;

  std::string id;
  std::string name;
  int64_t size{0};
  bool autoAccept{false};
  std::string description;
  int estInstallationDuration{0};
  int estPreparationDuration{0};
};

}  // namespace campaign

#endif
