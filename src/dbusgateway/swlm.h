#ifndef SWLM_H_
#define SWLM_H_

#include <dbus/dbus.h>
#include <string>

#include "config.h"

class SoftwareLoadingManagerProxy {
 public:
  SoftwareLoadingManagerProxy(const Config &config_in);
  void downloadComplete(const std::string &update_image, const std::string &signature);

 private:
  const Config &config;
  DBusError err;
  DBusConnection *conn;
};

#endif  // SWLM_H_
