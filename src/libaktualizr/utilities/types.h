#ifndef TYPES_H_
#define TYPES_H_
/** \file */

#include <algorithm>
#include <stdexcept>

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

inline std::istream& operator>>(std::istream& is, KeyType& kt) {
  std::string kt_str;

  is >> kt_str;
  std::transform(kt_str.begin(), kt_str.end(), kt_str.begin(), ::toupper);
  kt_str.erase(std::remove(kt_str.begin(), kt_str.end(), '"'), kt_str.end());

  if (kt_str == "RSA2048") {
    kt = KeyType::kRSA2048;
  } else if (kt_str == "RSA3072") {
    kt = KeyType::kRSA3072;
  } else if (kt_str == "RSA4096") {
    kt = KeyType::kRSA4096;
  } else if (kt_str == "ED25519") {
    kt = KeyType::kED25519;
  } else {
    kt = KeyType::kUnknown;
  }
  return is;
}

enum class CryptoSource { kFile = 0, kPkcs11, kAndroid };

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
  static struct tm CurrentTime();
  /** An invalid TimeStamp */
  TimeStamp() { ; }
  explicit TimeStamp(std::string rfc3339);
  explicit TimeStamp(struct tm time);
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

struct ResultCode {
  // These match the old enum representation
  // A lot of them were unused and have been dropped
  enum class Numeric {
    kOk = 0,
    /// Operation has already been processed
    kAlreadyProcessed = 1,
    /// Metadata verification failed
    kVerificationFailed = 3,
    /// Package installation failed
    kInstallFailed = 4,
    /// Package download failed
    kDownloadFailed = 5,
    /// SWM Internal integrity error
    kInternalError = 18,
    /// Other error
    kGeneralError = 19,
    // Install needs to be finalized (e.g: reboot)
    kNeedCompletion = 21,
    // Customer specific
    kCustomError = 22,
    // Unknown
    kUnknown = -1,
  };

  // note: intentionally *not* explicit, to make the common case easier
  ResultCode(ResultCode::Numeric in_num_code) : num_code(in_num_code) {}
  ResultCode(ResultCode::Numeric in_num_code, std::string text_code_in)
      : num_code(in_num_code), text_code(std::move(text_code_in)) {}

  bool operator==(const ResultCode& rhs) const { return num_code == rhs.num_code && toString() == rhs.toString(); }
  bool operator!=(const ResultCode& rhs) const { return !(*this == rhs); }
  friend std::ostream& operator<<(std::ostream& os, const ResultCode& result_code);

  Numeric num_code;
  std::string text_code;

  // Allows to have a numeric code with a default representation, but also with
  // any string representation
  std::string toString() const {
    if (text_code != "") {
      return text_code;
    }

    return std::string(string_repr.at(num_code));
  }

  // non-lossy reprensation for serialization
  std::string toRepr() const;
  static ResultCode fromRepr(const std::string& repr);

 private:
  static const std::map<Numeric, const char*> string_repr;
};

std::ostream& operator<<(std::ostream& os, const ResultCode& result_code);

struct InstallationResult {
  InstallationResult() = default;
  InstallationResult(ResultCode result_code_in, std::string description_in)
      : success(result_code_in.num_code == ResultCode::Numeric::kOk),
        result_code(std::move(result_code_in)),
        description(std::move(description_in)) {}
  InstallationResult(bool success_in, ResultCode result_code_in, std::string description_in)
      : success(success_in), result_code(std::move(result_code_in)), description(std::move(description_in)) {}

  Json::Value toJson() const;
  bool isSuccess() const { return success; };
  bool needCompletion() const { return result_code == ResultCode::Numeric::kNeedCompletion; }

  bool success{true};
  ResultCode result_code{ResultCode::Numeric::kOk};
  std::string description;
};

}  // namespace data

#endif
