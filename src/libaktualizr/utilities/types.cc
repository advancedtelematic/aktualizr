#include "utilities/types.h"

#include <stdexcept>
#include <utility>

TimeStamp TimeStamp::Now() {
  time_t raw_time;
  struct tm time_struct {};
  time(&raw_time);
  gmtime_r(&raw_time, &time_struct);
  char formatted[22];
  strftime(formatted, 22, "%Y-%m-%dT%H:%M:%SZ", &time_struct);
  return TimeStamp(formatted);
}

TimeStamp::TimeStamp(std::string rfc3339) {
  if (rfc3339.length() != 20 || rfc3339[19] != 'Z') {
    throw TimeStamp::InvalidTimeStamp();
  }
  time_ = rfc3339;
}

bool TimeStamp::IsValid() const { return time_.length() != 0; }

bool TimeStamp::IsExpiredAt(const TimeStamp &now) const {
  if (!IsValid()) {
    return true;
  }
  if (!now.IsValid()) {
    return true;
  }
  return *this < now;
}

bool TimeStamp::operator<(const TimeStamp &other) const { return IsValid() && other.IsValid() && time_ < other.time_; }

bool TimeStamp::operator>(const TimeStamp &other) const { return (other < *this); }

std::ostream &operator<<(std::ostream &os, const TimeStamp &t) {
  os << t.time_;
  return os;
}

namespace data {
Json::Value Package::toJson() {
  Json::Value json;
  json["name"] = name;
  json["version"] = version;
  return json;
}

Package Package::fromJson(const std::string &json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  Package package;
  package.name = json["name"].asString();
  package.version = json["version"].asString();
  return package;
}

const std::map<data::ResultCode::Numeric, const char *> data::ResultCode::string_repr{
    {ResultCode::Numeric::kOk, "OK"},
    {ResultCode::Numeric::kAlreadyProcessed, "ALREADY_PROCESSED"},
    {ResultCode::Numeric::kValidationFailed, "VALIDATION_FAILED"},
    {ResultCode::Numeric::kInstallFailed, "INSTALL_FAILED"},
    {ResultCode::Numeric::kInternalError, "INTERNAL_ERROR"},
    {ResultCode::Numeric::kGeneralError, "GENERAL_ERROR"},
    {ResultCode::Numeric::kNeedCompletion, "NEED_COMPLETION"},
    {ResultCode::Numeric::kCustomError, "CUSTOM_ERROR"},
    {ResultCode::Numeric::kUnknown, "UNKNOWN"},
};

std::string data::ResultCode::toRepr() const {
  std::string s = toString();

  if (s.find('\"') != std::string::npos) {
    throw std::runtime_error("Result code cannot contain double quotes");
  }

  return "\"" + s + "\"" + ":" + std::to_string(static_cast<int>(num_code));
}

ResultCode data::ResultCode::fromRepr(const std::string &repr) {
  size_t quote_n = repr.find('"');
  std::string s;
  size_t col_n;

  if (quote_n < repr.size() - 1) {
    size_t end_quote_n = repr.find('"', quote_n + 1);
    col_n = repr.find(':', end_quote_n + 1);
    s = repr.substr(quote_n + 1, end_quote_n - quote_n - 1);
  } else {
    // legacy format
    col_n = repr.find(':');
    s = repr.substr(0, col_n);
  }

  if (col_n >= repr.size() - 1) {
    return ResultCode(Numeric::kUnknown, s);
  }

  int num = std::stoi(repr.substr(col_n + 1));

  return ResultCode(static_cast<Numeric>(num), s);
}

Json::Value InstallationResult::toJson() const {
  Json::Value json;
  json["success"] = success;
  json["code"] = result_code.toString();
  json["description"] = description;
  return json;
}

std::ostream &operator<<(std::ostream &os, const ResultCode &result_code) {
  os << result_code.toRepr();
  return os;
}

}  // namespace data

// vim: set tabstop=2 shiftwidth=2 expandtab:
