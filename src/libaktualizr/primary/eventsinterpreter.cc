#include "eventsinterpreter.h"

#include <unistd.h>
#include <memory>
#include <utility>

#include "config/config.h"

EventsInterpreter::EventsInterpreter(
    const Config &config_in, std::shared_ptr<event::Channel> events_channel_in,
    std::shared_ptr<command::Channel> commands_channel_in,
    std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>> sig_in)
    : config(config_in),
      events_channel(std::move(events_channel_in)),
      commands_channel(std::move(commands_channel_in)),
      sig(std::move(sig_in)) {}

EventsInterpreter::~EventsInterpreter() {
  events_channel->close();
  thread.join();
}

void EventsInterpreter::interpret() { thread = std::thread(std::bind(&EventsInterpreter::run, this)); }

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_cycle(event::BaseEvent &event, const bool forever) {
  // Handles everything, in sequence
  if (event.variant == "SendDeviceDataComplete") {
    return std::make_shared<command::FetchMeta>();
  } else if (event.variant == "FetchMetaComplete") {
    return std::make_shared<command::CheckUpdates>();
  } else if (event.variant == "UpdateAvailable") {
    return std::make_shared<command::StartDownload>(dynamic_cast<event::UpdateAvailable &>(event).updates);
  } else if (event.variant == "DownloadComplete") {
    return std::make_shared<command::UptaneInstall>(dynamic_cast<event::DownloadComplete &>(event).updates);
  } else if (event.variant == "InstallComplete") {
    return std::make_shared<command::PutManifest>();
  }

  if (event.variant != "UptaneTimestampUpdated" && event.variant != "PutManifestComplete" && event.variant != "Error") {
    LOG_WARNING << "Unexpected event " << event.variant;
  }

  if (forever) {
    std::this_thread::sleep_for(std::chrono::seconds(config.uptane.polling_sec));
    return std::make_shared<command::FetchMeta>();
  } else {
    return std::make_shared<command::Shutdown>();
  }
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_manual(event::BaseEvent &event) {
  // Handles everything and then waits.
  if (event.variant == "SendDeviceDataComplete") {
    return std::make_shared<command::FetchMeta>();
  } else if (event.variant == "FetchMetaComplete") {
    return std::make_shared<command::CheckUpdates>();
  }

  // nothing to do, continue running
  return nullptr;
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_check(event::BaseEvent &event) {
  if (event.variant == "SendDeviceDataComplete") {
    return std::make_shared<command::FetchMeta>();
  } else if (event.variant == "FetchMetaComplete") {
    return std::make_shared<command::CheckUpdates>();
  } else if (event.variant == "UpdateAvailable") {
    return std::make_shared<command::Shutdown>();
  } else if (event.variant == "Error" || event.variant == "UptaneTimestampUpdated") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_WARNING << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_download(event::BaseEvent &event) {
  if (event.variant == "UpdateAvailable") {
    return std::make_shared<command::StartDownload>(dynamic_cast<event::UpdateAvailable &>(event).updates);
  } else if (event.variant == "DownloadComplete") {
    return std::make_shared<command::Shutdown>();
  } else if (event.variant == "Error") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_WARNING << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_install(event::BaseEvent &event) {
  if (event.variant == "UpdateAvailable") {
    return std::make_shared<command::UptaneInstall>(dynamic_cast<event::UpdateAvailable &>(event).updates);
  } else if (event.variant == "InstallComplete") {
    return std::make_shared<command::Shutdown>();
  } else if (event.variant == "Error") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_WARNING << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_campaigncheck(event::BaseEvent &event) {
  if (event.variant == "CampaignCheckComplete") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_ERROR << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

void EventsInterpreter::run() {
  std::shared_ptr<event::BaseEvent> event;
  auto running_mode = config.uptane.running_mode;

  while (*events_channel >> event) {
    LOG_INFO << "got " << event->variant << " event";
    if (sig) {
      (*sig)(event);
    }

    std::shared_ptr<command::BaseCommand> next_command;
    switch (running_mode) {
      case RunningMode::kFull:
        next_command = handle_cycle(*event, true);
        break;
      case RunningMode::kOnce:
        next_command = handle_cycle(*event, false);
        break;
      case RunningMode::kManual:
        next_command = handle_manual(*event);
        break;
      case RunningMode::kCheck:
        next_command = handle_check(*event);
        break;
      case RunningMode::kDownload:
        next_command = handle_download(*event);
        break;
      case RunningMode::kInstall:
        next_command = handle_install(*event);
        break;
      case RunningMode::kCampaignCheck:
        next_command = handle_campaigncheck(*event);
        break;
      default:
        LOG_ERROR << "Unknown running mode " << StringFromRunningMode(running_mode);
    }

    if (next_command == nullptr) {
      if (running_mode != RunningMode::kManual) {
        LOG_ERROR << "No command to run after event " << event->variant << " in mode "
                  << StringFromRunningMode(running_mode);
      }
    } else {
      *commands_channel << next_command;
    }
  }
}
