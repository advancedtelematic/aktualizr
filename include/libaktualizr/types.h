#ifndef TYPES_H_
#define TYPES_H_
/** \file */

#include <algorithm>
#include <stdexcept>

#include <json/json.h>
#include <boost/filesystem.hpp>

// kSharedCredReuse is intended solely for testing. It should not be used in
// production.
enum class ProvisionMode { kSharedCred = 0, kDeviceCred, kSharedCredReuse, kDefault };
std::ostream& operator<<(std::ostream& os, ProvisionMode mode);

enum class StorageType { kFileSystem = 0, kSqlite };
std::ostream& operator<<(std::ostream& os, StorageType stype);

/**
 * @brief The BasedPath class
 * Can represent an absolute or relative path, only readable through the BasePath::get() method.
 *
 * The intent is to avoid unintentional use of the "naked" relative path by
 * mandating a base directory for each instantiation.
 *
 * TODO has to be moved into Utils namespace
 */
class BasedPath {
 public:
  BasedPath(boost::filesystem::path p) : p_(std::move(p)) {}
  boost::filesystem::path get(const boost::filesystem::path &base) const;
  bool empty() const { return p_.empty(); }
  bool operator==(const BasedPath &b) const { return p_ == b.p_; }
  bool operator!=(const BasedPath &b) const { return !(*this == b); }

 private:
  boost::filesystem::path p_;
};

// Keep these in sync with AKIpUptaneKeyType ASN.1 definitions.
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

class PublicKey {
 public:
  PublicKey() = default;
  explicit PublicKey(const boost::filesystem::path &path);

  explicit PublicKey(Json::Value uptane_json);

  PublicKey(const std::string &value, KeyType type);

  std::string Value() const { return value_; }

  KeyType Type() const { return type_; }
  /**
   * Verify a signature using this public key
   */
  bool VerifySignature(const std::string &signature, const std::string &message) const;
  /**
   * Uptane Json representation of this public key.  Used in root.json
   * and during provisioning.
   */
  Json::Value ToUptane() const;

  std::string KeyId() const;
  bool operator==(const PublicKey &rhs) const;

  bool operator!=(const PublicKey &rhs) const { return !(*this == rhs); }

 private:
  // std::string can be implicitly converted to a Json::Value. Make sure that
  // the Json::Value constructor is not called accidentally.
  PublicKey(std::string);
  std::string value_;
  KeyType type_{KeyType::kUnknown};
};

/**
 * @brief The Hash class The hash of a file or Uptane metadata.
 * File hashes/checksums in Uptane include the length of the object,
 * in order to defeat infinite download attacks.
 */
class Hash {
 public:
  // order corresponds algorithm priority
  enum class Type { kSha256, kSha512, kUnknownAlgorithm };

  static Hash generate(Type type, const std::string &data);
  Hash(const std::string &type, const std::string &hash);
  Hash(Type type, const std::string &hash);

  bool HaveAlgorithm() const { return type_ != Type::kUnknownAlgorithm; }
  bool operator==(const Hash &other) const;
  bool operator!=(const Hash &other) const { return !operator==(other); }
  static std::string TypeString(Type type);
  std::string TypeString() const;
  Type type() const;
  std::string HashString() const { return hash_; }
  friend std::ostream &operator<<(std::ostream &os, const Hash &h);

  static std::string encodeVector(const std::vector<Hash> &hashes);
  static std::vector<Hash> decodeVector(std::string hashes_str);

 private:
  Type type_;
  std::string hash_;
};

std::ostream &operator<<(std::ostream &os, const Hash &h);


// timestamp, compatible with tuf
class TimeStamp {
 public:
  static TimeStamp Now();
  static struct tm CurrentTime();
  /** An invalid TimeStamp */
  TimeStamp() {}
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
  Json::Value toJson() const;
  static Package fromJson(const std::string& /*json_str*/);
};

struct ResultCode {
  // Keep these in sync with AKInstallationResultCode ASN.1 definitions.
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
  // any string representation. This is specifically useful for campaign success
  // analysis, because the device installation report concatenates the
  // individual ECU ResultCodes.
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
      : success(result_code_in.num_code == ResultCode::Numeric::kOk ||
                result_code_in.num_code == ResultCode::Numeric::kAlreadyProcessed),
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

namespace Uptane {

struct InstalledImageInfo {
  InstalledImageInfo() : name{""} {}
  InstalledImageInfo(std::string name_in, uint64_t len_in, std::string hash_in)
      : name(std::move(name_in)), len(len_in), hash(std::move(hash_in)) {}
  std::string name;
  uint64_t len{0};
  std::string hash;
};

class HardwareIdentifier {
 public:
  // https://github.com/advancedtelematic/ota-tuf/blob/master/libtuf/src/main/scala/com/advancedtelematic/libtuf/data/TufDataType.scala
  static const int kMinLength = 0;
  static const int kMaxLength = 200;

  static HardwareIdentifier Unknown() { return HardwareIdentifier("Unknown"); }
  explicit HardwareIdentifier(const std::string &hwid) : hwid_(hwid) {
    /* if (hwid.length() < kMinLength) {
      throw std::out_of_range("Hardware Identifier too short");
    } */
    if (kMaxLength < hwid.length()) {
      throw std::out_of_range("Hardware Identifier too long");
    }
  }

  std::string ToString() const { return hwid_; }

  bool operator==(const HardwareIdentifier &rhs) const { return hwid_ == rhs.hwid_; }
  bool operator!=(const HardwareIdentifier &rhs) const { return !(*this == rhs); }

  bool operator<(const HardwareIdentifier &rhs) const { return hwid_ < rhs.hwid_; }
  friend std::ostream &operator<<(std::ostream &os, const HardwareIdentifier &hwid);
  friend struct std::hash<Uptane::HardwareIdentifier>;

 private:
  std::string hwid_;
};

std::ostream &operator<<(std::ostream &os, const HardwareIdentifier &hwid);

class EcuSerial {
 public:
  // https://github.com/advancedtelematic/ota-tuf/blob/master/libtuf/src/main/scala/com/advancedtelematic/libtuf/data/TufDataType.scala
  static const int kMinLength = 1;
  static const int kMaxLength = 64;

  static EcuSerial Unknown() { return EcuSerial("Unknown"); }
  explicit EcuSerial(const std::string &ecu_serial) : ecu_serial_(ecu_serial) {
    if (ecu_serial.length() < kMinLength) {
      throw std::out_of_range("Ecu serial identifier is too short");
    }
    if (kMaxLength < ecu_serial.length()) {
      throw std::out_of_range("Ecu serial identifier is too long");
    }
  }

  std::string ToString() const { return ecu_serial_; }

  bool operator==(const EcuSerial &rhs) const { return ecu_serial_ == rhs.ecu_serial_; }
  bool operator!=(const EcuSerial &rhs) const { return !(*this == rhs); }

  bool operator<(const EcuSerial &rhs) const { return ecu_serial_ < rhs.ecu_serial_; }
  friend std::ostream &operator<<(std::ostream &os, const EcuSerial &ecu_serial);
  friend struct std::hash<Uptane::EcuSerial>;

 private:
  std::string ecu_serial_;
};

std::ostream &operator<<(std::ostream &os, const EcuSerial &ecu_serial);

using EcuMap = std::map<EcuSerial, HardwareIdentifier>;

class Target {
 public:
  // From Uptane metadata
  Target(std::string filename, const Json::Value &content);
  // Internal use only. Only used for reading installed_versions list and by
  // various tests.
  Target(std::string filename, EcuMap ecus, std::vector<Hash> hashes, uint64_t length, std::string correlation_id = "");

  static Target Unknown();

  const EcuMap &ecus() const { return ecus_; }
  std::string filename() const { return filename_; }
  std::string sha256Hash() const;
  std::string sha512Hash() const;
  const std::vector<Hash> &hashes() const { return hashes_; }
  const std::vector<HardwareIdentifier> &hardwareIds() const { return hwids_; }
  std::string custom_version() const { return custom_["version"].asString(); }
  Json::Value custom_data() const { return custom_; }
  void updateCustom(Json::Value &custom) { custom_ = custom; }
  std::string correlation_id() const { return correlation_id_; }
  void setCorrelationId(std::string correlation_id) { correlation_id_ = std::move(correlation_id); }
  uint64_t length() const { return length_; }
  bool IsValid() const { return valid; }
  std::string uri() const { return uri_; }
  void setUri(std::string uri) { uri_ = std::move(uri); }
  bool MatchHash(const Hash &hash) const;

  void InsertEcu(const std::pair<EcuSerial, HardwareIdentifier> &pair) { ecus_.insert(pair); }

  bool IsForEcu(const EcuSerial &ecuIdentifier) const {
    return (std::find_if(ecus_.cbegin(), ecus_.cend(),
                         [&ecuIdentifier](const std::pair<EcuSerial, HardwareIdentifier> &pair) {
                           return pair.first == ecuIdentifier;
                         }) != ecus_.cend());
  }

  /**
   * Is this an OSTree target?
   * OSTree targets need special treatment because the hash doesn't represent
   * the contents of the update itself, instead it is the hash (name) of the
   * root commit object.
   */
  bool IsOstree() const;
  std::string type() const { return type_; }

  // Comparison is usually not meaningful. Use MatchTarget instead.
  bool operator==(const Target &t2) = delete;
  bool MatchTarget(const Target &t2) const;
  Json::Value toDebugJson() const;
  friend std::ostream &operator<<(std::ostream &os, const Target &t);
  InstalledImageInfo getTargetImageInfo() const { return {filename(), length(), sha256Hash()}; }

 private:
  bool valid{true};
  std::string filename_;
  std::string type_;
  EcuMap ecus_;  // Director only
  std::vector<Hash> hashes_;
  std::vector<HardwareIdentifier> hwids_;  // Image repo only
  Json::Value custom_;
  uint64_t length_{0};
  std::string correlation_id_;
  std::string uri_;

  std::string hashString(Hash::Type type) const;
};

std::ostream &operator<<(std::ostream &os, const Target &t);

} // namespace Uptane

struct SecondaryInfo {
  SecondaryInfo() : serial(Uptane::EcuSerial::Unknown()), hw_id(Uptane::HardwareIdentifier::Unknown()) {}
  SecondaryInfo(Uptane::EcuSerial serial_in, Uptane::HardwareIdentifier hw_id_in, std::string type_in,
                PublicKey pub_key_in, std::string extra_in)
      : serial(std::move(serial_in)),
        hw_id(std::move(hw_id_in)),
        type(std::move(type_in)),
        pub_key(std::move(pub_key_in)),
        extra(std::move(extra_in)) {}

  Uptane::EcuSerial serial;
  Uptane::HardwareIdentifier hw_id;
  std::string type;
  PublicKey pub_key;

  std::string extra;
};

#endif
