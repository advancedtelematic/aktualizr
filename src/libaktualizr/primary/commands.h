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
  virtual std::string toJson();
  virtual std::string toJson(Json::Value json);
  virtual ~BaseCommand() = default;
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
};

class FetchMeta : public BaseCommand {
 public:
  FetchMeta();
};

class CheckUpdates : public BaseCommand {
 public:
  CheckUpdates();
};

class SendDeviceData : public BaseCommand {
 public:
  SendDeviceData();
};

class StartDownload : public BaseCommand {
 public:
  explicit StartDownload(std::vector<Uptane::Target> updates_in);
  static StartDownload fromJson(const std::string& json_str);

  std::vector<Uptane::Target> updates;
  std::string toJson() override;
};

class UptaneInstall : public BaseCommand {
 public:
  explicit UptaneInstall(std::vector<Uptane::Target> /*packages_in*/);
  static UptaneInstall fromJson(const std::string& json_str);

  std::vector<Uptane::Target> packages;
};
}  // namespace command
#endif
