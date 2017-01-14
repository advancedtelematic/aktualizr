#include "interpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include "events.h"

#include "config.h"

Interpreter::Interpreter(const Config &config_in,
                         command::Channel *commands_channel_in,
                         event::Channel *events_channel_in)
    : config(config_in),
      sota(config),
      commands_channel(commands_channel_in),
      events_channel(events_channel_in) {}

Interpreter::~Interpreter() { delete thread; }

void Interpreter::interpret() {
  thread = new boost::thread(boost::bind(&Interpreter::run, this));
}

void Interpreter::run() {
  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    if (command->variant == "GetUpdateRequests") {
      LOGGER_LOG(LVL_info, "got GetUpdateRequests command");
      std::vector<data::UpdateRequest> updates = sota.getAvailableUpdates();
      if (updates.size()) {
        LOGGER_LOG(LVL_info, "got new updates, sending UpdatesReceived event");
        *events_channel << boost::shared_ptr<event::UpdatesReceived>(
            new event::UpdatesReceived(updates));
      } else {
        LOGGER_LOG(LVL_info, "no new updates, sending NoUpdateRequests event");
        *events_channel << boost::shared_ptr<event::NoUpdateRequests>(
            new event::NoUpdateRequests());
      }

    } else if (command->variant == "StartDownload") {
      LOGGER_LOG(LVL_info, "got StartDownload command");
      Json::Value status = sota.downloadUpdate(
          static_cast<command::StartDownload *>(command.get())
              ->update_request_id);
      data::DownloadComplete download_complete;
      download_complete.update_id = status["updateId"].asString();
      download_complete.update_image =
          config.device.packages_dir + download_complete.update_id;
      LOGGER_LOG(LVL_info,
                 "download complete, sending  DownloadComplete event");
      *events_channel << boost::shared_ptr<event::DownloadComplete>(
          new event::DownloadComplete(download_complete));

    } else if (command->variant == "SendUpdateReport") {
      LOGGER_LOG(LVL_info, "got SendUpdateReport command");
      data::UpdateReport update_report =
          static_cast<command::SendUpdateReport *>(command.get())
              ->update_report;
      sota.reportUpdateResult(update_report);
    }
  }
}
