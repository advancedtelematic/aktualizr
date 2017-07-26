#ifndef EVENTS_H_
#define EVENTS_H_

#include <boost/shared_ptr.hpp>
#include <map>

#include <json/json.h>

#include "channel.h"
#include "types.h"
#ifdef BUILD_OSTREE
#include "ostree.h"
#endif

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

class UpdateAvailable : public BaseEvent {
 public:
  data::UpdateAvailable update_vailable;
  UpdateAvailable(const data::UpdateAvailable& ua_in);
  virtual std::string toJson();
  static UpdateAvailable fromJson(const std::string& json_str);
};

class DownloadComplete : public BaseEvent {
 public:
  DownloadComplete(const data::DownloadComplete& dc_in);
  data::DownloadComplete download_complete;
  virtual std::string toJson();
  static DownloadComplete fromJson(const std::string& json_str);
};

class InstalledSoftwareNeeded : public BaseEvent {
 public:
  InstalledSoftwareNeeded();
  virtual std::string toJson();
};

class UptaneTimestampUpdated : public BaseEvent {
 public:
  UptaneTimestampUpdated();
  virtual std::string toJson();
};

#ifdef BUILD_OSTREE
class UptaneTargetsUpdated : public BaseEvent {
 public:
  std::vector<OstreePackage> packages;
  virtual std::string toJson();

  UptaneTargetsUpdated(std::vector<OstreePackage> packages_in);
};
#endif
};

#endif
