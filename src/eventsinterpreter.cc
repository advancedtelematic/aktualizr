#include "eventsinterpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"


EventsInterpreter::EventsInterpreter(const Config& config_in,
                                     event::Channel* events_channel_in,
                                     command::Channel* commands_channel_in)
    : config(config_in),
      events_channel(events_channel_in),
      commands_channel(commands_channel_in),
      gateway_manager(config, commands_channel) {}

EventsInterpreter::~EventsInterpreter() { delete thread; }

void EventsInterpreter::interpret() {
  thread = new boost::thread(boost::bind(&EventsInterpreter::run, this));
}

void EventsInterpreter::run() {

  boost::shared_ptr<event::BaseEvent> event;
  while (*events_channel >> event) {

    gateway_manager.processEvents(event);

    if (event->variant == "DownloadComplete") {
      LOGGER_LOG(LVL_info, "got DownloadComplete event");
    } else if (event->variant == "UpdatesReceived") {
      LOGGER_LOG(LVL_info, "got UpdatesReceived event");
      std::vector<data::UpdateRequest> updates =
          static_cast<event::UpdatesReceived*>(event.get())->update_requests;
      for (std::vector<data::UpdateRequest>::iterator update = updates.begin();
           update != updates.end(); ++update) {
        LOGGER_LOG(LVL_info, "sending  StartDownload command");
        *commands_channel << boost::shared_ptr<command::StartDownload>(
            new command::StartDownload(update->requestId));
      }
    }
  }
}
