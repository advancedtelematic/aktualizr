#ifndef COMANDS_H_
#define COMANDS_H_

#include <string>

#include <json/json.h>
#include <picojson.h>

#include "uptane/tuf.h"
#include "utilities/channel.h"
#include "utilities/types.h"

namespace command {

struct BaseCommand {
  std::string variant;
  Json::Value toJson();
  static std::shared_ptr<BaseCommand> fromPicoJson(const picojson::value& json);
  template <typename T>
  T* toChild() {
    return static_cast<T*>(this);
  }
};
using Channel = Channel<std::shared_ptr<BaseCommand> >;

class Shutdown : public BaseCommand {
 public:
  Shutdown();
  std::string toJson();
};

class GetUpdateRequests : public BaseCommand {
 public:
  GetUpdateRequests();
  std::string toJson();
};

class StartDownload : public BaseCommand {
 public:
  explicit StartDownload(data::UpdateRequestId ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static StartDownload fromJson(const std::string& json_str);
};

class AbortDownload : public BaseCommand {
 public:
  explicit AbortDownload(data::UpdateRequestId ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static AbortDownload fromJson(const std::string& json_str);
};

class SendUpdateReport : public BaseCommand {
 public:
  explicit SendUpdateReport(data::UpdateReport ureport_in);
  data::UpdateReport update_report;
  std::string toJson();
  static SendUpdateReport fromJson(const std::string& json_str);
};

class UptaneInstall : public BaseCommand {
 public:
  explicit UptaneInstall(std::vector<Uptane::Target> /*packages_in*/);
  std::vector<Uptane::Target> packages;
};
}  // namespace command
#endif
