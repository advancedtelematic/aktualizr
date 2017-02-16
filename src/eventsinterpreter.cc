#include "eventsinterpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"

EventsInterpreter::EventsInterpreter(const Config& config_in, event::Channel* events_channel_in,
                                     command::Channel* commands_channel_in)
    : config(config_in),
      events_channel(events_channel_in),
      commands_channel(commands_channel_in),
      gateway_manager(config, commands_channel) {}

EventsInterpreter::~EventsInterpreter() {
  events_channel->close();
  if (!thread.try_join_for(boost::chrono::seconds(10))) {
    LOGGER_LOG(LVL_error, "join()-ing DBusGateway thread timed out");
  }
}

void EventsInterpreter::interpret() { thread = boost::thread(boost::bind(&EventsInterpreter::run, this)); }

void EventsInterpreter::run() {
  boost::shared_ptr<event::BaseEvent> event;
  while (*events_channel >> event) {
    LOGGER_LOG(LVL_info, "got " << event->variant << " event");
    gateway_manager.processEvents(event);

    if (event->variant == "DownloadComplete") {
      event::DownloadComplete* download_complete_event = static_cast<event::DownloadComplete*>(event.get());
      data::OperationResult operation_result;
      operation_result.id = download_complete_event->download_complete.update_id;
      operation_result.result_code = data::OK;
      operation_result.result_text = "Downloaded";
      data::UpdateReport update_report;
      update_report.update_id = download_complete_event->download_complete.update_id;
      update_report.operation_results.push_back(operation_result);
      *commands_channel << boost::make_shared<command::SendUpdateReport>(update_report);
    } else if (event->variant == "UpdatesReceived") {
      std::vector<data::UpdateRequest> updates = static_cast<event::UpdatesReceived*>(event.get())->update_requests;
      for (std::vector<data::UpdateRequest>::iterator update = updates.begin(); update != updates.end(); ++update) {
        LOGGER_LOG(LVL_info, "sending  StartDownload command");
        *commands_channel << boost::shared_ptr<command::StartDownload>(new command::StartDownload(update->requestId));
      }
    } else if (event->variant == "UpdateAvailable") {
      *commands_channel << boost::shared_ptr<command::StartDownload>(
          new command::StartDownload(static_cast<event::UpdateAvailable*>(event.get())->update_vailable.update_id));
    }
  }
}
