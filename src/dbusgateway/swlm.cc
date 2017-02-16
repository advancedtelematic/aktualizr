#include "swlm.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "logger.h"

SoftwareLoadingManagerProxy::SoftwareLoadingManagerProxy(const Config &config_in) : config(config_in) {
  dbus_error_init(&err);
  conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    LOGGER_LOG(LVL_error, "Dbus Connection Error: " << err.message);
    dbus_error_free(&err);
    return;
  }
}

void SoftwareLoadingManagerProxy::downloadComplete(const std::string &update_image, const std::string &signature) {
  DBusMessage *msg;
  DBusMessageIter args;

  msg = dbus_message_new_method_call(config.dbus.software_manager.c_str(), config.dbus.software_manager_path.c_str(),
                                     (config.dbus.software_manager).c_str(), "downloadComplete");

  dbus_message_iter_init_append(msg, &args);

  const char *update_image_c = update_image.c_str();
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &update_image_c);

  const char *signature_c = signature.c_str();
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &signature_c);

  DBusMessage *reply;
  dbus_error_init(&err);
  reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
  if (dbus_error_is_set(&err)) {
    LOGGER_LOG(LVL_error, "Error " << err.name << ":" << err.message);
    dbus_message_unref(msg);
    return;
  }
  if (reply) {
    dbus_message_unref(reply);
  }

  dbus_message_unref(msg);
}
