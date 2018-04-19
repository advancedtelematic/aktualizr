#ifndef COMANDS_H_
#define COMANDS_H_

#include <string>

#include <json/json.h>
#include <picojson.h>

#include "channel.h"
#include "uptane/tuf.h"
#include "utilities/types.h"

namespace command {

struct BaseCommand {
  std::string variant;
  Json::Value toJson();
  static std::shared_ptr<BaseCommand> fromPicoJson(const picojson::value& json);
  template <typename T>
  T* toChild() {
    return (T*)this;
  }
};
typedef Channel<std::shared_ptr<BaseCommand> > Channel;

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
  StartDownload(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static StartDownload fromJson(const std::string& json_str);
};

class AbortDownload : public BaseCommand {
 public:
  AbortDownload(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static AbortDownload fromJson(const std::string& json_str);
};

class SendUpdateReport : public BaseCommand {
 public:
  SendUpdateReport(const data::UpdateReport& ureport_in);
  data::UpdateReport update_report;
  std::string toJson();
  static SendUpdateReport fromJson(const std::string& json_str);
};

class UptaneInstall : public BaseCommand {
 public:
  UptaneInstall(std::vector<Uptane::Target>);
  std::vector<Uptane::Target> packages;
};
}  // namespace command
#endif
