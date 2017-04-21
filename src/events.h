#ifndef EVENTS_H_
#define EVENTS_H_

#include <boost/shared_ptr.hpp>
#include <map>

#include <json/json.h>

#include "channel.h"
#include "types.h"

namespace event {

class BaseEvent {
 public:
  std::string variant;
  Json::Value toBaseJson();
  virtual std::string toJson() = 0;
};
typedef Channel<boost::shared_ptr<BaseEvent> > Channel;

class Error : public BaseEvent {
 public:
  std::string message;
  Error(const std::string&);
  virtual std::string toJson();
  static Error fromJson(const std::string&);
};

class Authenticated : public BaseEvent {
 public:
  Authenticated();
  virtual std::string toJson();
};

class NotAuthenticated : public BaseEvent {
 public:
  NotAuthenticated();
  virtual std::string toJson();
};

class AlreadyAuthenticated : public BaseEvent {
 public:
  AlreadyAuthenticated();
  virtual std::string toJson();
};

class UpdatesReceived : public BaseEvent {
 public:
  std::vector<data::UpdateRequest> update_requests;
  UpdatesReceived(const std::vector<data::UpdateRequest>& ur);
  virtual std::string toJson();
  static UpdatesReceived fromJson(const std::string& json_str);
};

class UpdateAvailable : public BaseEvent {
 public:
  data::UpdateAvailable update_vailable;
  UpdateAvailable(const data::UpdateAvailable& ua_in);
  virtual std::string toJson();
  static UpdateAvailable fromJson(const std::string& json_str);
};

class NoUpdateRequests : public BaseEvent {
 public:
  NoUpdateRequests();
  virtual std::string toJson();
};

class FoundInstalledPackages : public BaseEvent {
 public:
  FoundInstalledPackages(const std::vector<data::Package>&);
  std::vector<data::Package> packages;
  virtual std::string toJson();
  static FoundInstalledPackages fromJson(const std::string& json_str);
};

class FoundSystemInfo : public BaseEvent {
 public:
  FoundSystemInfo(const std::string& info_in);
  std::string info;
  virtual std::string toJson();
  static FoundSystemInfo fromJson(const std::string& json_str);
};

class DownloadingUpdate : public BaseEvent {
 public:
  DownloadingUpdate(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  virtual std::string toJson();
  static DownloadingUpdate fromJson(const std::string& json_str);
};

class DownloadComplete : public BaseEvent {
 public:
  DownloadComplete(const data::DownloadComplete& dc_in);
  data::DownloadComplete download_complete;
  virtual std::string toJson();
  static DownloadComplete fromJson(const std::string& json_str);
};

class DownloadFailed : public BaseEvent {
 public:
  DownloadFailed(const data::UpdateRequestId& ur_in, const std::string& message);
  data::UpdateRequestId update_request_id;
  std::string message;
  virtual std::string toJson();
  static DownloadFailed fromJson(const std::string& json_str);
};

class InstallingUpdate : public BaseEvent {
 public:
  InstallingUpdate(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  virtual std::string toJson();
  static InstallingUpdate fromJson(const std::string& json_str);
};

class InstallComplete : public BaseEvent {
 public:
  InstallComplete(const data::OperationResult& install_result_in);
  data::OperationResult install_result;
  virtual std::string toJson();
  static InstallComplete fromJson(const std::string& json_str);
};

class InstallFailed : public BaseEvent {
 public:
  InstallFailed(const data::OperationResult& install_result_in);
  data::OperationResult install_result;
  virtual std::string toJson();
  static InstallFailed fromJson(const std::string& json_str);
};

class UpdateReportSent : public BaseEvent {
 public:
  UpdateReportSent();
  virtual std::string toJson();
};

class InstalledPackagesSent : public BaseEvent {
 public:
  InstalledPackagesSent();
  virtual std::string toJson();
};

class InstalledSoftwareSent : public BaseEvent {
 public:
  InstalledSoftwareSent();
  virtual std::string toJson();
};

class SystemInfoSent : public BaseEvent {
 public:
  SystemInfoSent();
  virtual std::string toJson();
};

class InstalledSoftwareNeeded : public BaseEvent {
 public:
  InstalledSoftwareNeeded();
  virtual std::string toJson();
};

typedef std::map<std::string, Json::Value> TufMetaMap;
class UptaneTimestampUpdated : public BaseEvent {
 public:
  UptaneTimestampUpdated();
};
};

#endif
