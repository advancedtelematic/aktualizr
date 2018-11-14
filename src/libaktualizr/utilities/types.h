#ifndef TYPES_H_
#define TYPES_H_
/** \file */

#include <json/json.h>
#include <boost/filesystem.hpp>
#include <stdexcept>

// Keep these int sync with AKIpUptaneKeyType ASN.1 definitions
enum class KeyType {
  kED25519 = 0,
  kFirstKnown = kED25519,
  kRSA2048,
  kRSA3072,
  kRSA4096,
  kLastKnown = kRSA4096,
  kUnknown = 0xff
};

inline std::ostream& operator<<(std::ostream& os, const KeyType kt) {
  std::string kt_str;
  switch (kt) {
    case KeyType::kRSA2048:
      kt_str = "RSA2048";
      break;
    case KeyType::kRSA3072:
      kt_str = "RSA3072";
      break;
    case KeyType::kRSA4096:
      kt_str = "RSA4096";
      break;
    case KeyType::kED25519:
      kt_str = "ED25519";
      break;
    default:
      kt_str = "unknown";
      break;
  }
  os << '"' << kt_str << '"';
  return os;
}

inline std::istream& operator>>(std::istream& is, KeyType& kt) {
  std::string kt_str;

  is >> kt_str;

  if (kt_str == "\"RSA2048\"") {
    kt = KeyType::kRSA2048;
  } else if (kt_str == "\"RSA3072\"") {
    kt = KeyType::kRSA3072;
  } else if (kt_str == "\"RSA4096\"") {
    kt = KeyType::kRSA4096;
  } else if (kt_str == "\"ED25519\"") {
    kt = KeyType::kED25519;
  } else {
    kt = KeyType::kUnknown;
  }
  return is;
}

/** Execution mode to run aktualizr in. */
enum class RunningMode {
  /** Fully automated mode. Regularly checks for updates and downloads and
   *  installs automatically. Runs indefinitely. */
  kFull = 0,
  /** One complete cycle. Checks once for updates, downloads and installs
   *  anything found, and then shuts down. */
  kOnce,
  /** Only check for an existing campaign related to the device */
  kCampaignCheck,
  /** Only accept an existing campaign */
  kCampaignAccept,
  /** Only reject an existing campaign */
  kCampaignReject,
  /** Only check for updates. Sends a manifest and device data, checks for
   *  updates, and then shuts down. */
  kCheck,
  /** Download any available updates and then shut down. */
  kDownload,
  /** Install any available updates and then shut down. Does not requite network
   *  connectivity. */
  kInstall,
  /** Completely manual operation. Send commands via the aktualizr class's API.
   *  Runs indefinitely until a Shutdown command is received. */
  kManual,
};

RunningMode RunningModeFromString(const std::string& mode);
std::string StringFromRunningMode(RunningMode mode);

enum class CryptoSource { kFile = 0, kPkcs11 };

inline std::ostream& operator<<(std::ostream& os, CryptoSource cs) {
  std::string cs_str;
  switch (cs) {
    case CryptoSource::kFile:
      cs_str = "file";
      break;
    case CryptoSource::kPkcs11:
      cs_str = "pkcs11";
      break;
    default:
      cs_str = "unknown";
      break;
  }
  os << '"' << cs_str << '"';
  return os;
}

// timestamp, compatible with tuf
class TimeStamp {
 public:
  static TimeStamp Now();
  /** An invalid TimeStamp */
  TimeStamp() { ; }
  explicit TimeStamp(std::string rfc3339);
  bool IsExpiredAt(const TimeStamp& now) const;
  bool IsValid() const;
  std::string ToString() const { return time_; }
  bool operator<(const TimeStamp& other) const;
  bool operator>(const TimeStamp& other) const;
  friend std::ostream& operator<<(std::ostream& os, const TimeStamp& t);
  bool operator==(const TimeStamp& rhs) const { return time_ == rhs.time_; }

  class InvalidTimeStamp : public std::domain_error {
   public:
    InvalidTimeStamp() : std::domain_error("invalid timestamp") {}
    ~InvalidTimeStamp() noexcept override = default;
  };

 private:
  std::string time_;
};

std::ostream& operator<<(std::ostream& os, const TimeStamp& t);

/// General data structures.
namespace data {

using UpdateRequestId = std::string;
struct Package {
  std::string name;
  std::string version;
  Json::Value toJson();
  static Package fromJson(const std::string& /*json_str*/);
};

/// Result of an update.
enum class UpdateResultCode {
  /// Operation executed successfully
  kOk = 0,
  /// Operation has already been processed
  kAlreadyProcessed = 1,
  /// Dependency failure during package install, upgrade, or removal
  kDependencyFailure = 2,
  /// Update image integrity has been compromised
  kValidationFailed = 3,
  /// Package installation failed
  kInstallFailed = 4,
  /// Package upgrade failed
  kUpgradeFailed = 5,
  /// Package removal failed
  kRemovalFailed = 6,
  /// The module loader could not flash its managed module
  kFlashFailed = 7,
  /// Partition creation failed
  kCreatePartitionFailed = 8,
  /// Partition deletion failed
  kDeletePartitionFailed = 9,
  /// Partition resize failed
  kResizePartitionFailed = 10,
  /// Partition write failed
  kWritePartitionFailed = 11,
  /// Partition patching failed
  kPatchPartitionFailed = 12,
  /// User declined the update
  kUserDeclined = 13,
  /// Software was blacklisted
  kSoftwareBlacklisted = 14,
  /// Ran out of disk space
  kDiskFull = 15,
  /// Software package not found
  kNotFound = 16,
  /// Tried to downgrade to older version
  kOldVersion = 17,
  /// SWM Internal integrity error
  kInternalError = 18,
  /// Other error
  kGeneralError = 19,
  /// Updating process in progress
  kInProgress = 20,
  // Install needs to be finalized (e.g: reboot)
  kNeedCompletion = 21,
};

typedef std::pair<UpdateResultCode, std::string> InstallOutcome;

struct OperationResult {
  OperationResult() : result_code(UpdateResultCode::kOk) {}
  OperationResult(std::string id_in, UpdateResultCode result_code_in, std::string result_text_in);
  OperationResult(std::string id_in, InstallOutcome outcome_in);
  std::string id;
  UpdateResultCode result_code{};
  std::string result_text;
  Json::Value toJson() const;
  bool isSuccess() const {
    return result_code == UpdateResultCode::kOk || result_code == UpdateResultCode::kAlreadyProcessed;
  };
  InstallOutcome toOutcome() const;
  static OperationResult fromJson(const std::string& json_str);
  static OperationResult fromOutcome(const std::string& id, const InstallOutcome& outcome);
};

}  // namespace data

#endif
