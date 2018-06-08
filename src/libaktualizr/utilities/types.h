#ifndef TYPES_H_
#define TYPES_H_

#include <json/json.h>
#include <boost/filesystem.hpp>

// Keep these int sync with AKIpUptaneKeyType ASN.1 definitions
enum class KeyType { ED25519 = 0, FirstKnown = ED25519, RSA2048, RSA4096, LastKnown = RSA4096, Unknown = 0xff };

inline std::ostream& operator<<(std::ostream& os, const KeyType kt) {
  std::string kt_str;
  switch (kt) {
    case KeyType::RSA2048:
      kt_str = "RSA2048";
      break;
    case KeyType::RSA4096:
      kt_str = "RSA4096";
      break;
    case KeyType::ED25519:
      kt_str = "ED25519";
      break;
    default:
      kt_str = "unknown";
      break;
  }
  os << '"' << kt_str << '"';
  return os;
}

enum class CryptoSource { File = 0, Pkcs11 };

inline std::ostream& operator<<(std::ostream& os, CryptoSource cs) {
  std::string cs_str;
  switch (cs) {
    case CryptoSource::File:
      cs_str = "file";
      break;
    case CryptoSource::Pkcs11:
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

struct UpdateAvailable {
  std::string update_id;
  std::string signature;
  std::string description;
  bool request_confirmation{};
  uint64_t size{};
  Json::Value toJson();
  static UpdateAvailable fromJson(const std::string& json_str);
  static UpdateAvailable fromJson(const Json::Value& json);
};

struct DownloadComplete {
  std::string update_id;
  std::string update_image;
  std::string signature;
  Json::Value toJson();
  static DownloadComplete fromJson(const std::string& json_str);
};

enum class UpdateResultCode {
  /// Operation executed successfully
  OK = 0,
  /// Operation has already been processed
  ALREADY_PROCESSED,
  /// Dependency failure during package install, upgrade, or removal
  DEPENDENCY_FAILURE,
  /// Update image integrity has been compromised
  VALIDATION_FAILED,
  /// Package installation failed
  INSTALL_FAILED,
  /// Package upgrade failed
  UPGRADE_FAILED,
  /// Package removal failed
  REMOVAL_FAILED,
  /// The module loader could not flash its managed module
  FLASH_FAILED,
  /// Partition creation failed
  CREATE_PARTITION_FAILED,
  /// Partition deletion failed
  DELETE_PARTITION_FAILED,
  /// Partition resize failed
  RESIZE_PARTITION_FAILED,
  /// Partition write failed
  WRITE_PARTITION_FAILED,
  /// Partition patching failed
  PATCH_PARTITION_FAILED,
  /// User declined the update
  USER_DECLINED,
  /// Software was blacklisted
  SOFTWARE_BLACKLISTED,
  /// Ran out of disk space
  DISK_FULL,
  /// Software package not found
  NOT_FOUND,
  /// Tried to downgrade to older version
  OLD_VERSION,
  /// SWM Internal integrity error
  INTERNAL_ERROR,
  /// Other error
  GENERAL_ERROR,
  /// Updating process in progress
  IN_PROGRESS
};

typedef std::pair<UpdateResultCode, std::string> InstallOutcome;

struct UpdateReport;
struct OperationResult {
  OperationResult() : result_code(UpdateResultCode::OK) {}
  OperationResult(std::string id_in, UpdateResultCode result_code_in, std::string result_text_in);
  std::string id;
  UpdateResultCode result_code{};
  std::string result_text;
  Json::Value toJson();
  UpdateReport toReport();
  bool isSuccess() {
    return result_code == UpdateResultCode::OK || result_code == UpdateResultCode::ALREADY_PROCESSED;
  };
  InstallOutcome toOutcome();
  static OperationResult fromJson(const std::string& json_str);
  static OperationResult fromOutcome(const std::string& id, const InstallOutcome& outcome);
};

struct UpdateReport {
  UpdateRequestId update_id;
  std::vector<data::OperationResult> operation_results;
  Json::Value toJson();
  static UpdateReport fromJson(const std::string& json_str);
};
}  // namespace data

#endif
