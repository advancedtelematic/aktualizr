#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <future>
#include <string>

#include "json/json.h"

#include "uptane/secondaryconfig.h"
#include "uptane/tuf.h"
#include "utilities/events.h"

/* Json snippet returned by sendMetaXXX():
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */

namespace Uptane {

class SecondaryInterface {
 public:
  explicit SecondaryInterface(SecondaryConfig sconfig_in) : sconfig(std::move(sconfig_in)) {}
  virtual ~SecondaryInterface() = default;
  virtual EcuSerial getSerial() { return Uptane::EcuSerial(sconfig.ecu_serial); }
  virtual Uptane::HardwareIdentifier getHwId() { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  virtual PublicKey getPublicKey() = 0;

  virtual Json::Value getManifest() = 0;
  virtual bool putMetadata(const RawMetaPack& meta_pack) = 0;
  virtual int32_t getRootVersion(bool director) = 0;
  virtual bool putRoot(const std::string& root, bool director) = 0;

  virtual std::future<bool> sendFirmwareAsync(const std::shared_ptr<std::string>& data) = 0;
  const SecondaryConfig sconfig;
  void addEventsChannel(std::shared_ptr<event::Channel> channel) { events_channel = std::move(channel); }

 protected:
  template <class T, class... Args>
  void sendEvent(Args&&... args) {
    std::shared_ptr<event::BaseEvent> event = std::make_shared<T>(std::forward<Args>(args)...);
    if (events_channel) {
      (*events_channel)(std::move(event));
    } else if (event->variant == "Error") {
      auto err_event = dynamic_cast<event::Error*>(event.get());
      LOG_WARNING << "got Error event: " << err_event->message;
    } else if (event->variant != "DownloadProgressReport") {
      LOG_INFO << "got " << event->variant << " event";
    }
  }

 private:
  std::shared_ptr<event::Channel> events_channel;
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYINTERFACE_H
