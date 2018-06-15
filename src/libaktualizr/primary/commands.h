#ifndef COMANDS_H_
#define COMANDS_H_

#include <string>

#include <json/json.h>

#include "uptane/tuf.h"
#include "utilities/channel.h"
#include "utilities/types.h"

namespace command {

class BaseCommand {
 public:
  std::string variant;
  virtual Json::Value toJson();
  virtual ~BaseCommand() = default;
  static std::shared_ptr<BaseCommand> fromJson(const Json::Value& json);
  template <typename T>
  T* toChild() {
    return static_cast<T*>(this);
  }

 protected:
  BaseCommand(std::string v) : variant(std::move(v)) {}
  virtual Json::Value toJson(const Json::Value& json);
};
using Channel = Channel<std::shared_ptr<BaseCommand> >;

class Shutdown : public BaseCommand {
 public:
  Shutdown() : BaseCommand("Shutdown") {}

 private:
  explicit Shutdown(const Json::Value& /* json */) : Shutdown() {}

  friend BaseCommand;
};

class FetchMeta : public BaseCommand {
 public:
  FetchMeta() : BaseCommand("FetchMeta") {}

 private:
  explicit FetchMeta(const Json::Value& /* json */) : FetchMeta() {}

  friend BaseCommand;
};

class CheckUpdates : public BaseCommand {
 public:
  CheckUpdates() : BaseCommand("CheckUpdates") {}

 private:
  explicit CheckUpdates(const Json::Value& /* json */) : CheckUpdates() {}

  friend BaseCommand;
};

class SendDeviceData : public BaseCommand {
  friend BaseCommand;

 public:
  SendDeviceData() : BaseCommand("SendDeviceData") {}

 private:
  explicit SendDeviceData(const Json::Value& /* json */) : SendDeviceData() {}
};

class StartDownload : public BaseCommand {
 public:
  explicit StartDownload(std::vector<Uptane::Target> updates_in);

  std::vector<Uptane::Target> updates;
  Json::Value toJson() override;

 private:
  explicit StartDownload(const Json::Value& json);

  friend BaseCommand;
};

class UptaneInstall : public BaseCommand {
 public:
  explicit UptaneInstall(std::vector<Uptane::Target> packages_in);

  std::vector<Uptane::Target> packages;
  Json::Value toJson() override;

 private:
  explicit UptaneInstall(const Json::Value& json);

  friend BaseCommand;
};
}  // namespace command
#endif
