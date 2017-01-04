#ifndef EVENTS_H_
#define EVENTS_H_

#include <json/json.h>

#include "types.h"

namespace event {

class BaseEvent {
 public:
  std::string variant;
  Json::Value toJson();
};

class Error : public BaseEvent {
 public:
  std::string message;
  Error(const std::string&);
  std::string toJson();
  static Error fromJson(const std::string&);
};

class Authenticated : public BaseEvent {
 public:
  Authenticated();
  std::string toJson();
};

class NotAuthenticated : public BaseEvent {
 public:
  NotAuthenticated();
  std::string toJson();
};

class AlreadyAuthenticated : public BaseEvent {
 public:
  AlreadyAuthenticated();
  std::string toJson();
};

class UpdatesReceived : public BaseEvent {
 public:
  std::vector<data::UpdateRequest> update_requests;
  UpdatesReceived(const std::vector<data::UpdateRequest>& ur);
  std::string toJson();
  static UpdatesReceived fromJson(const std::string& json_str);
};

class UpdateAvailable : public BaseEvent {
 public:
  data::UpdateAvailable update_vailable;
  UpdateAvailable(const data::UpdateAvailable& ua_in);
  std::string toJson();
  static UpdateAvailable fromJson(const std::string& json_str);
};

class NoUpdateRequests : public BaseEvent {
 public:
  NoUpdateRequests();
  std::string toJson();
};

class FoundInstalledPackages : public BaseEvent {
 public:
  FoundInstalledPackages(const std::vector<data::Package>&);
  std::vector<data::Package> packages;
  std::string toJson();
  static FoundInstalledPackages fromJson(const std::string& json_str);
};

class FoundSystemInfo : public BaseEvent {
 public:
  FoundSystemInfo(const std::string& info_in);
  std::string info;
  std::string toJson();
  static FoundSystemInfo fromJson(const std::string& json_str);
};

class DownloadingUpdate : public BaseEvent {
 public:
  DownloadingUpdate(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static DownloadingUpdate fromJson(const std::string& json_str);
};

class DownloadComplete : public BaseEvent {
 public:
  DownloadComplete(const data::DownloadComplete& dc_in);
  data::DownloadComplete download_complete;
  std::string toJson();
  static DownloadComplete fromJson(const std::string& json_str);
};

class DownloadFailed : public BaseEvent {
 public:
  DownloadFailed(const data::UpdateRequestId& ur_in,
                 const std::string& message);
  data::UpdateRequestId update_request_id;
  std::string message;
  std::string toJson();
  static DownloadFailed fromJson(const std::string& json_str);
};

class InstallingUpdate : public BaseEvent {
 public:
  InstallingUpdate(const data::UpdateRequestId& ur_in);
  data::UpdateRequestId update_request_id;
  std::string toJson();
  static InstallingUpdate fromJson(const std::string& json_str);
};

class InstallComplete : public BaseEvent {
 public:
  InstallComplete(const data::UpdateReport& ureport_in);
  data::UpdateReport update_report;
  std::string toJson();
  static InstallComplete fromJson(const std::string& json_str);
};

class InstallFailed : public BaseEvent {
 public:
  InstallFailed(const data::UpdateReport& ureport_in);
  data::UpdateReport update_report;
  std::string toJson();
  static InstallFailed fromJson(const std::string& json_str);
};

class UpdateReportSent : public BaseEvent {
 public:
  UpdateReportSent();
  std::string toJson();
};

class InstalledPackagesSent : public BaseEvent {
 public:
  InstalledPackagesSent();
  std::string toJson();
};

class InstalledSoftwareSent : public BaseEvent {
 public:
  InstalledSoftwareSent();
  std::string toJson();
};

class SystemInfoSent : public BaseEvent {
 public:
  SystemInfoSent();
  std::string toJson();
};

class InstalledSoftwareNeeded : public BaseEvent {
 public:
  InstalledSoftwareNeeded();
  std::string toJson();
};
};

#endif