#include "dbusgateway.h"

#include <boost/shared_ptr.hpp>
#include "logger.h"
#include "types.h"

DbusGateway::DbusGateway(const Config& config_in, command::Channel* command_channel_in)
    : stop(false), config(config_in), command_channel(command_channel_in), swlm(config) {
  dbus_threads_init_default();
  dbus_error_init(&err);
  conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    LOGGER_LOG(LVL_error, "D-Bus Connection Error: " << err.message);
    dbus_error_free(&err);
    return;
  }

  int ret = dbus_bus_request_name(conn, config_in.dbus.interface.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
    LOGGER_LOG(LVL_error, "Cannot request D-Bus name '" << config_in.dbus.interface << "' as primary owner. D-Bus communication disabled");
    return;
  }
  thread = boost::thread(boost::bind(&DbusGateway::run, this));
}

DbusGateway::~DbusGateway() {
  stop = true;
  if (!thread.try_join_for(boost::chrono::seconds(10))) {
    LOGGER_LOG(LVL_error, "join()-ing DBusGateway thread timed out");
  }
  dbus_bus_release_name(conn, config.dbus.interface.c_str(), NULL);
  dbus_connection_unref(conn);
}

void DbusGateway::processEvent(const boost::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadComplete") {
    event::DownloadComplete* download_complete_event = static_cast<event::DownloadComplete*>(event.get());
    swlm.downloadComplete(download_complete_event->download_complete.update_image,
                          download_complete_event->download_complete.signature);
    fireDownloadCompleteEvent(download_complete_event->download_complete);
  } else if (event->variant == "InstalledSoftwareNeeded") {
    fireInstalledSoftwareNeededEvent();
  } else if (event->variant == "UpdateAvailable") {
    event::UpdateAvailable* update_available_event = static_cast<event::UpdateAvailable*>(event.get());
    fireUpdateAvailableEvent(update_available_event->update_vailable);
  }
}

void DbusGateway::fireInstalledSoftwareNeededEvent() {
  DBusMessage* msg;
  msg = dbus_message_new_signal(config.dbus.path.c_str(), config.dbus.interface.c_str(), "InstalledSoftwareNeeded");

  dbus_connection_send(conn, msg, NULL);

  dbus_message_unref(msg);
}

void DbusGateway::fireUpdateAvailableEvent(const data::UpdateAvailable& update_available) {
  DBusMessage* msg;
  DBusMessageIter iter;

  msg = dbus_message_new_signal(config.dbus.path.c_str(), config.dbus.interface.c_str(), "UpdateAvailable");

  dbus_message_iter_init_append(msg, &iter);

  DBusMessageIter sub_iter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &sub_iter);
  const char* update_id = update_available.update_id.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &update_id);
  const char* description = update_available.description.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &description);
  const char* signature = update_available.signature.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &signature);
  unsigned long long size = update_available.size;
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_UINT64, &size);

  dbus_message_iter_close_container(&iter, &sub_iter);

  dbus_connection_send(conn, msg, NULL);

  dbus_message_unref(msg);
}

void DbusGateway::fireDownloadCompleteEvent(const data::DownloadComplete& download_complete) {
  DBusMessage* msg;
  DBusMessageIter iter;

  msg = dbus_message_new_signal(config.dbus.path.c_str(), config.dbus.interface.c_str(), "DownloadComplete");
  dbus_message_iter_init_append(msg, &iter);

  DBusMessageIter sub_iter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, NULL, &sub_iter);
  const char* update_id = download_complete.update_id.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &update_id);
  const char* update_image = download_complete.update_image.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &update_image);
  const char* signature = download_complete.signature.c_str();
  dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &signature);
  dbus_message_iter_close_container(&iter, &sub_iter);

  dbus_connection_send(conn, msg, NULL);
  dbus_message_unref(msg);
}

void DbusGateway::run() {
  DBusMessage* msg;
  char* string_param;
  DBusMessageIter args;

  while (true) {
    dbus_connection_read_write(conn, 0);
    msg = dbus_connection_pop_message(conn);
    if (NULL == msg) {
      boost::this_thread::sleep_for(boost::chrono::seconds(1));
      if (stop) {
        return;
      }
      continue;
    }

    if (dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "initiateDownload") ||
        dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "abortDownload") ||
        dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "updateReport")) {
      if (!dbus_message_iter_init(msg, &args) || DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) {
        LOGGER_LOG(LVL_error, "D-Bus method initiateDownload called with wrong arguments");
        dbus_message_unref(msg);
        continue;
      } else {
        dbus_message_iter_get_basic(&args, &string_param);
      }
    }
    if (dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "initiateDownload")) {
      *command_channel << boost::shared_ptr<command::StartDownload>(new command::StartDownload(string_param));
    } else if (dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "abortDownload")) {
      *command_channel << boost::shared_ptr<command::AbortDownload>(new command::AbortDownload(string_param));
    } else if (dbus_message_is_method_call(msg, config.dbus.interface.c_str(), "updateReport")) {
      data::UpdateReport update_report;
      update_report.update_id = string_param;
      if (dbus_message_iter_next(&args) && DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&args)) {
        DBusMessageIter operation_result_args;
        int results_count = dbus_message_iter_get_element_count(&args);
        dbus_message_iter_recurse(&args, &operation_result_args);
        do {
          try {
            update_report.operation_results.push_back(getOperationResult(&operation_result_args));
          } catch (...) {
            LOGGER_LOG(LVL_error, "D-Bus method 'updateReport' called with wrong arguments");
          }
          dbus_message_iter_next(&operation_result_args);
          results_count--;
        } while (results_count);

      } else {
        LOGGER_LOG(LVL_error, "D-Bus method called with wrong arguments");
        dbus_message_unref(msg);
        continue;
      }
      *command_channel << boost::shared_ptr<command::SendUpdateReport>(new command::SendUpdateReport(update_report));
    }

    int message_type = dbus_message_get_type(msg);
    LOGGER_LOG(LVL_trace, "Got D-Bus message type:" << dbus_message_type_to_string(message_type));
    if (message_type ==DBUS_MESSAGE_TYPE_METHOD_CALL) {
      DBusMessage *reply = dbus_message_new_method_return(msg);
      dbus_bool_t ok = dbus_connection_send(conn, reply, NULL);
      if (!ok) {
        LOGGER_LOG(LVL_error, "D-Bus method send failed");
      }
      dbus_connection_flush(conn);

      dbus_message_unref(reply);
    }
    dbus_message_unref(msg);
  }
}

data::OperationResult DbusGateway::getOperationResult(DBusMessageIter* iter) {
  DBusMessageIter subiter, dict_iter, variant_iter;
  char* string_param;
  int int_param;
  data::OperationResult result;
  int params = dbus_message_iter_get_element_count(iter);
  dbus_message_iter_recurse(iter, &subiter);
  if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(iter)) {
    while (params) {
      dbus_message_iter_recurse(&subiter, &dict_iter);
      dbus_message_iter_get_basic(&dict_iter, &string_param);
      dbus_message_iter_next(&dict_iter);
      dbus_message_iter_recurse(&dict_iter, &variant_iter);
      if (std::string(string_param) == "id") {
        dbus_message_iter_get_basic(&variant_iter, &string_param);
        result.id = string_param;
        dbus_message_iter_next(&subiter);
      } else if (std::string(string_param) == "result_code") {
        dbus_message_iter_get_basic(&variant_iter, &int_param);
        result.result_code = (data::UpdateResultCode)int_param;
        dbus_message_iter_next(&subiter);
      } else if (std::string(string_param) == "result_text") {
        dbus_message_iter_get_basic(&variant_iter, &string_param);
        result.result_text = string_param;
        dbus_message_iter_next(&subiter);
      }
      params--;
    }
  }
  return result;
}
