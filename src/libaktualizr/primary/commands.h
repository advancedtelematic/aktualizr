#ifndef COMANDS_H_
#define COMANDS_H_
/** \file */

#include <string>

#include <json/json.h>

#include "uptane/tuf.h"
#include "utilities/channel.h"
#include "utilities/types.h"

/**
 * Aktualizr control commands.
 */
namespace command {

/**
 * Base class for all command objects.
 */
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

/**
 * Shut aktualizr down and return control from the main thread.
 */
class Shutdown : public BaseCommand {
 public:
  Shutdown() : BaseCommand("Shutdown") {}
};

/**
 * Fetch metadata from the server.
 *
 * Also sends network information and current manifest to the server.
 */
class FetchMeta : public BaseCommand {
 public:
  FetchMeta() : BaseCommand("FetchMeta") {}
};

/**
 * Check updates for validity.
 *
 * Generated automatically after receiving metadata from the server (as with
 * FetchMeta) and thus should not need to be used manually.
 */
class CheckUpdates : public BaseCommand {
 public:
  CheckUpdates() : BaseCommand("CheckUpdates") {}
};

/**
 * Send device data to the server.
 *
 * Sends hardware information, a list of installed packages, and network
 * information. Automatically generates a FetchMeta command upon completion.
 */
class SendDeviceData : public BaseCommand {
 public:
  SendDeviceData() : BaseCommand("SendDeviceData") {}
};

/**
 * Send a manifest to the server.
 *
 * This is normally done as part of a FetchMeta command and thus should not need
 * to be used manually.
 */
class PutManifest : public BaseCommand {
 public:
  PutManifest() : BaseCommand("PutManifest") {}
};

/**
 * Download a set of Uptane targets.
 *
 * The targets should be in the same format as provided in the UpdateAvailable
 * event.
 */
class StartDownload : public BaseCommand {
 public:
  explicit StartDownload(std::vector<Uptane::Target> updates_in);

  std::vector<Uptane::Target> updates;
  Json::Value toJson() override;

 private:
  explicit StartDownload(const Json::Value& json);

  friend BaseCommand;
};

/**
 * Install a set of Uptane targets.
 *
 * The targets should be in the same format as provided in the UpdateAvailable
 * event.
 */
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
