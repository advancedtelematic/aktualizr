#ifndef RESULTS_H_
#define RESULTS_H_
/** \file */

#include <string>
#include <vector>

#include "campaign/campaign.h"
#include "uptane/tuf.h"

/**
 * Container for information about available campaigns.
 */
class CampaignCheckResult {
 public:
  explicit CampaignCheckResult(std::vector<campaign::Campaign> campaigns_in) : campaigns(std::move(campaigns_in)) {}
  std::vector<campaign::Campaign> campaigns;
};

/**
 * Status of an update.
 */
enum class UpdateStatus {
  /* Updates are available for ECUs known to aktualizr. */
  kUpdatesAvailable = 0,
  /* No updates are available for ECUs known to aktualizr. */
  kNoUpdatesAvailable,
  /* There was an error checking for updates. */
  kError,
};

/**
 * Container for information about available updates.
 */
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

/**
 * Status of an update download.
 */
enum class DownloadStatus {
  /* Update was downloaded successfully. */
  kSuccess = 0,
  /* Some targets in the update were downloaded successfully, but some were not. */
  kPartialSuccess,
  /* There are no targets to download for this update. */
  kNothingToDownload,
  /* There was an error downloading targets. */
  kError,
};

/**
 * Container for information about downloading an update.
 */
class DownloadResult {
 public:
  DownloadResult() = default;
  DownloadResult(std::vector<Uptane::Target> updates_in, DownloadStatus status_in, std::string message_in)
      : updates(std::move(updates_in)), status(status_in), message(std::move(message_in)) {}
  std::vector<Uptane::Target> updates;
  DownloadStatus status{DownloadStatus::kNothingToDownload};
  std::string message;
};

/**
 * Installation report for a given target on a given ECU.
 */
class InstallReport {
 public:
  InstallReport(Uptane::Target update_in, Uptane::EcuSerial serial_in, data::OperationResult status_in)
      : update(std::move(update_in)), serial(std::move(serial_in)), status(std::move(status_in)) {}
  Uptane::Target update;
  Uptane::EcuSerial serial;
  data::OperationResult status;
};

/**
 * Container for information about installing an update.
 */
class InstallResult {
 public:
  InstallResult() = default;
  explicit InstallResult(std::vector<InstallReport> reports_in) : reports(std::move(reports_in)) {}
  std::vector<InstallReport> reports;
};

#endif  // RESULTS_H_
