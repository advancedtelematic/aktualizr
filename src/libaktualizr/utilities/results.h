#ifndef RESULTS_H_
#define RESULTS_H_

#include <string>
#include <vector>

#include "campaign/campaign.h"
#include "uptane/tuf.h"

class CampaignCheckResult {
 public:
  explicit CampaignCheckResult(std::vector<campaign::Campaign> campaigns_in) : campaigns(std::move(campaigns_in)) {}
  std::vector<campaign::Campaign> campaigns;
};

enum class UpdateStatus {
  kUpdatesAvailable = 0,
  kNoUpdatesAvailable,
  kError,
};

class UpdateCheckResult {
 public:
  UpdateCheckResult() = default;
  UpdateCheckResult(std::vector<Uptane::Target> updates_in, unsigned int ecus_count_in, UpdateStatus status_in,
                    std::string message_in)
      : updates(std::move(updates_in)), ecus_count(ecus_count_in), status(status_in), message(std::move(message_in)) {}
  std::vector<Uptane::Target> updates;
  unsigned int ecus_count{0};
  UpdateStatus status{UpdateStatus::kNoUpdatesAvailable};
  std::string message;
};

enum class DownloadStatus {
  kSuccess = 0,
  kPartialSuccess,
  kNothingToDownload,
  kError,
};

class DownloadResult {
 public:
  DownloadResult() = default;
  DownloadResult(std::vector<Uptane::Target> updates_in, DownloadStatus status_in, std::string message_in)
      : updates(std::move(updates_in)), status(status_in), message(std::move(message_in)) {}
  std::vector<Uptane::Target> updates;
  DownloadStatus status{DownloadStatus::kNothingToDownload};
  std::string message;
};

#endif  // RESULTS_H_
