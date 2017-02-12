#ifndef DBUSGATEWAY_H_
#define DBUSGATEWAY_H_

#include <dbus/dbus.h>

#include "commands.h"
#include "config.h"
#include "events.h"
#include "gateway.h"
#include "swlm.h"

class DbusGateway : public Gateway {
 public:
  DbusGateway(const Config &config_in, command::Channel *command_channel_in);
  virtual ~DbusGateway();
  virtual void processEvent(const boost::shared_ptr<event::BaseEvent> &event);
  void fireInstalledSoftwareNeededEvent();
  void fireUpdateAvailableEvent(const data::UpdateAvailable &update_available);
  void fireDownloadCompleteEvent(const data::DownloadComplete &download_complete);
  void run();

 private:
  data::OperationResult getOperationResult(DBusMessageIter *);
  Config config;
  command::Channel *command_channel;
  DBusError err;
  DBusConnection *conn;
  SoftwareLoadingManagerProxy swlm;
};
#endif /* DBUSGATEWAY_H_ */
