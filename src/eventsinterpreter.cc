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
    LOG_ERROR << "join()-ing EventsInterpreter thread timed out";
  }
}

void EventsInterpreter::interpret() { thread = boost::thread(boost::bind(&EventsInterpreter::run, this)); }

void EventsInterpreter::run() {
  boost::shared_ptr<event::BaseEvent> event;
  while (*events_channel >> event) {
    LOG_INFO << "got " << event->variant << " event";
    gateway_manager.processEvents(event);

    if (event->variant == "UpdateAvailable") {
      *commands_channel << boost::make_shared<command::StartDownload>(
          static_cast<event::UpdateAvailable*>(event.get())->update_vailable.update_id);
    } else if (event->variant == "UptaneTargetsUpdated") {
      *commands_channel << boost::make_shared<command::UptaneInstall>(
          static_cast<event::UptaneTargetsUpdated*>(event.get())->packages);
    }
    if (event->variant == "UptaneTimestampUpdated") {
      // These events indicates the end of pooling cycle
      if (!config.uptane.polling) {
        // the option --pooling-once is set, so we need to exit now
        *commands_channel << boost::make_shared<command::Shutdown>();
      }
    }
  }
}
