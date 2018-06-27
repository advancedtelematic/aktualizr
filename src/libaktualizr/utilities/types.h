#ifndef TYPES_H_
#define TYPES_H_

#include <json/json.h>
#include <boost/filesystem.hpp>

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

enum class CryptoSource { kFile = 0, kPkcs11 };

enum class RunningMode { kFull = 0, kOnce, kCheck, kDownload, kInstall };
RunningMode RunningModeFromString(const std::string& mode);
std::string StringFromRunningMode(RunningMode mode);

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

namespace data {

using UpdateRequestId = std::string;
struct Package {
  std::string name;
  std::string version;
  Json::Value toJson();
  static Package fromJson(const std::string& /*json_str*/);
};

enum class UpdateResultCode {
  /// Operation executed successfully
  kOk = 0,
  /// Operation has already been processed
  kAlreadyProcessed,
  /// Dependency failure during package install, upgrade, or removal
  kDependencyFailure,
  /// Update image integrity has been compromised
  kValidationFailed,
  /// Package installation failed
  kInstallFailed,
  /// Package upgrade failed
  kUpgradeFailed,
  /// Package removal failed
  kRemovalFailed,
  /// The module loader could not flash its managed module
  kFlashFailed,
  /// Partition creation failed
  kCreatePartitionFailed,
  /// Partition deletion failed
  kDeletePartitionFailed,
  /// Partition resize failed
  kResizePartitionFailed,
  /// Partition write failed
  kWritePartitionFailed,
  /// Partition patching failed
  kPatchPartitionFailed,
  /// User declined the update
  kUserDeclined,
  /// Software was blacklisted
  kSoftwareBlacklisted,
  /// Ran out of disk space
  kDiskFull,
  /// Software package not found
  kNotFound,
  /// Tried to downgrade to older version
  kOldVersion,
  /// SWM Internal integrity error
  kInternalError,
  /// Other error
  kGeneralError,
  /// Updating process in progress
  kInProgress
};

typedef std::pair<UpdateResultCode, std::string> InstallOutcome;

struct OperationResult {
  OperationResult() : result_code(UpdateResultCode::kOk) {}
  OperationResult(std::string id_in, UpdateResultCode result_code_in, std::string result_text_in);
  std::string id;
  UpdateResultCode result_code{};
  std::string result_text;
  Json::Value toJson();
  bool isSuccess() {
    return result_code == UpdateResultCode::kOk || result_code == UpdateResultCode::kAlreadyProcessed;
  };
  InstallOutcome toOutcome();
  static OperationResult fromJson(const std::string& json_str);
  static OperationResult fromOutcome(const std::string& id, const InstallOutcome& outcome);
};

}  // namespace data

#endif
