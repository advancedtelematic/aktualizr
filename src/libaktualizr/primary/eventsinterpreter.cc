#include "eventsinterpreter.h"
#include <unistd.h>
#include <memory>
#include <utility>

#include "config/config.h"

EventsInterpreter::EventsInterpreter(const Config& config_in, std::shared_ptr<event::Channel> events_channel_in,
                                     std::shared_ptr<command::Channel> commands_channel_in)
    : config(config_in),
      events_channel(std::move(std::move(events_channel_in))),
      commands_channel(std::move(std::move(commands_channel_in))),
      gateway_manager(config, commands_channel) {}

EventsInterpreter::~EventsInterpreter() {
  events_channel->close();
  thread.join();
}

void EventsInterpreter::interpret() { thread = std::thread(std::bind(&EventsInterpreter::run, this)); }

void EventsInterpreter::run() {
  std::shared_ptr<event::BaseEvent> event;
  while (*events_channel >> event) {
    LOG_INFO << "got " << event->variant << " event";
    gateway_manager.processEvents(event);

    if (event->variant == "UpdateAvailable") {
      *commands_channel << std::make_shared<command::StartDownload>(
          dynamic_cast<event::UpdateAvailable*>(event.get())->update_vailable.update_id);
    } else if (event->variant == "UptaneTargetsUpdated") {
      *commands_channel << std::make_shared<command::UptaneInstall>(
          dynamic_cast<event::UptaneTargetsUpdated*>(event.get())->packages);
    }
    if (event->variant == "UptaneTimestampUpdated") {
      // These events indicates the end of pooling cycle
      if (!config.uptane.polling) {
        // the option --poll-once is set, so we need to exit now
        *commands_channel << std::make_shared<command::Shutdown>();
      }
    }
  }
}
