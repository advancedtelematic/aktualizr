#include "commandinterpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "events.h"
#include "sotahttpclient.h"

CommandInterpreter::CommandInterpreter(SotaClient *sota_in, command::Channel *commands_channel_in,
                                       event::Channel *events_channel_in)
    : sota(sota_in), commands_channel(commands_channel_in), events_channel(events_channel_in) {}

CommandInterpreter::~CommandInterpreter() { delete thread; }

void CommandInterpreter::interpret() {
  boost::shared_ptr<command::BaseCommand> command;
  while (*commands_channel >> command) {
    LOGGER_LOG(LVL_info, "got " + command->variant + " command");
    if (command->variant == "Authenticate") {
      if (((SotaHttpClient *)sota)->authenticate()) {
        *events_channel << boost::make_shared<event::Authenticated>();
      } else {
        *events_channel << boost::make_shared<event::NotAuthenticated>();
      }
    } else if (command->variant == "GetUpdateRequests") {
      std::vector<data::UpdateRequest> updates = static_cast<SotaHttpClient *>(sota)->getAvailableUpdates();
      if (updates.size()) {
        LOGGER_LOG(LVL_info, "got new updates, sending UpdatesReceived event");
        *events_channel << boost::shared_ptr<event::UpdatesReceived>(new event::UpdatesReceived(updates));
      } else {
        LOGGER_LOG(LVL_info, "no new updates, sending NoUpdateRequests event");
        *events_channel << boost::shared_ptr<event::NoUpdateRequests>(new event::NoUpdateRequests());
      }
    } else if (command->variant == "StartDownload") {
      command::StartDownload *start_download_command = command->toChild<command::StartDownload>();
      sota->startDownload(start_download_command->update_request_id);
    } else if (command->variant == "SendUpdateReport") {
      command::SendUpdateReport *send_update_report_command = command->toChild<command::SendUpdateReport>();
      sota->sendUpdateReport(send_update_report_command->update_report);
    }
  }
}
