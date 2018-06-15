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

  if (config.uptane.running_mode == RunningMode::kDownload || config.uptane.running_mode == RunningMode::kInstall) {
    *commands_channel << std::make_shared<command::CheckUpdates>();
  } else if (config.uptane.running_mode == RunningMode::kCheck) {
    *commands_channel << std::make_shared<command::FetchMeta>();
  } else {
    *commands_channel << std::make_shared<command::SendDeviceData>();
  }

  while (*events_channel >> event) {
    LOG_INFO << "got " << event->variant << " event";
    gateway_manager.processEvents(event);

    if (event->variant == "SendDeviceDataComplete") {
      *commands_channel << std::make_shared<command::FetchMeta>();
    } else if (event->variant == "FetchMetaComplete") {
      *commands_channel << std::make_shared<command::CheckUpdates>();
    } else if (event->variant == "UpdateAvailable") {
      if (config.uptane.running_mode == RunningMode::kInstall) {
        *commands_channel << std::make_shared<command::UptaneInstall>(
            dynamic_cast<event::UpdateAvailable*>(event.get())->updates);
      } else if (config.uptane.running_mode != RunningMode::kCheck) {
        *commands_channel << std::make_shared<command::StartDownload>(
            dynamic_cast<event::UpdateAvailable*>(event.get())->updates);
      } else {
        *commands_channel << std::make_shared<command::Shutdown>();
      }
    } else if (event->variant == "DownloadComplete") {
      if (config.uptane.running_mode == RunningMode::kFull || config.uptane.running_mode == RunningMode::kOnce) {
        *commands_channel << std::make_shared<command::UptaneInstall>(
            dynamic_cast<event::DownloadComplete*>(event.get())->updates);
      } else {
        *commands_channel << std::make_shared<command::Shutdown>();
      }
    }
    if (event->variant == "UptaneTimestampUpdated" || event->variant == "InstallComplete" ||
        event->variant == "Error") {
      // These events indicates the end of pooling cycle
      if (config.uptane.running_mode == RunningMode::kFull) {
        std::this_thread::sleep_for(std::chrono::seconds(config.uptane.polling_sec));
        *commands_channel << std::make_shared<command::FetchMeta>();
      } else {
        *commands_channel << std::make_shared<command::Shutdown>();
      }
    }
  }
}
