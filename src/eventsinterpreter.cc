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
      if (config.device.package_manager != PMOFF) {
        event::DownloadComplete* download_complete_event = static_cast<event::DownloadComplete*>(event.get());
        *commands_channel << boost::make_shared<command::StartInstall>(
            download_complete_event->download_complete.update_id);
      }
    } else if (event->variant == "UpdatesReceived") {
      std::vector<data::UpdateRequest> updates = static_cast<event::UpdatesReceived*>(event.get())->update_requests;
      for (std::vector<data::UpdateRequest>::iterator update = updates.begin(); update != updates.end(); ++update) {
        LOGGER_LOG(LVL_info, "sending  StartDownload command");
        *commands_channel << boost::shared_ptr<command::StartDownload>(new command::StartDownload(update->requestId));
      }
    } 
#ifdef BUILD_OSTREE
    else if(event->variant == "UptaneTargetsUpdated") {
      *commands_channel << boost::make_shared<command::OstreeInstall>(
          static_cast<event::UptaneTargetsUpdated*>(event.get())->packages);
    }
#endif
    else if (event->variant == "UpdateAvailable") {
      *commands_channel << boost::make_shared<command::StartDownload>(
          static_cast<event::UpdateAvailable*>(event.get())->update_vailable.update_id);
    } else if (event->variant == "InstallComplete") {
      *commands_channel << boost::shared_ptr<command::SendUpdateReport>(
          new command::SendUpdateReport(static_cast<event::InstallComplete*>(event.get())->install_result.toReport()));
    } else if (event->variant == "InstallFailed") {
      *commands_channel << boost::shared_ptr<command::SendUpdateReport>(
          new command::SendUpdateReport(static_cast<event::InstallFailed*>(event.get())->install_result.toReport()));
    }
  }
}
