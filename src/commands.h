#ifndef COMANDS_H_
#define COMANDS_H_

#include <json/json.h>
#include <picojson.h>
#include <boost/shared_ptr.hpp>
#include <string>

#include "channel.h"
#include "types.h"

namespace command {

struct BaseCommand {
  std::string variant;
  Json::Value toJson();
  static boost::shared_ptr<BaseCommand> fromPicoJson(
      const picojson::value& json);
  template <typename T>
  T* toChild() {
    return (T*)this;
  };
};
typedef Channel<boost::shared_ptr<BaseCommand> > Channel;

class Authenticate : public BaseCommand {
 public:
  Authenticate(const data::ClientCredentials& client_credentials_in);
  data::ClientCredentials client_credentials;
  std::string toJson();
  static Authenticate fromJson(const std::string& json_str);
};

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

class ListInstalledPackages : public BaseCommand {
 public:
  ListInstalledPackages();
  std::string toJson();
};

class ListSystemInfo : public BaseCommand {
 public:
  ListSystemInfo();
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

class StartInstall : public BaseCommand {
 public:
  StartInstall(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static StartInstall fromJson(const std::string& json_str);
};

class SendInstalledPackages : public BaseCommand {
 public:
  SendInstalledPackages(const std::vector<data::Package>&);
  std::vector<data::Package> packages;
  std::string toJson();
  static SendInstalledPackages fromJson(const std::string& json_str);
};

class SendInstalledSoftware : public BaseCommand {
 public:
  SendInstalledSoftware(const data::InstalledSoftware&);
  data::InstalledSoftware installed_software;
  std::string toJson();
  static SendInstalledSoftware fromJson(const std::string& json_str);
};

class SendSystemInfo : public BaseCommand {
 public:
  SendSystemInfo();
  std::string toJson();
};

class SendUpdateReport : public BaseCommand {
 public:
  SendUpdateReport(const data::UpdateReport& update_report_in);
  data::UpdateReport update_report;
  std::string toJson();
  static SendUpdateReport fromJson(const std::string& json_str);
};
};
#endif
