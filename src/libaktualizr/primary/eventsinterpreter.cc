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
    auto update_available = dynamic_cast<event::UpdateAvailable &>(event);
    pending_ecus = update_available.ecus_count;
    return std::make_shared<command::StartDownload>(update_available.updates);
  } else if (event.variant == "DownloadComplete") {
    return std::make_shared<command::UptaneInstall>(dynamic_cast<event::DownloadComplete &>(event).updates);
  } else if (event.variant == "InstallComplete") {
    auto install_complete = dynamic_cast<event::InstallComplete &>(event);
    LOG_INFO << "ECU " << install_complete.serial << " installation has finished";
    pending_ecus--;
    if (pending_ecus == 0) {
      return std::make_shared<command::PutManifest>();
    } else {
      return nullptr;
    }
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
    auto update_available = dynamic_cast<event::UpdateAvailable &>(event);
    pending_ecus = update_available.ecus_count;
    return std::make_shared<command::UptaneInstall>(update_available.updates);
  } else if (event.variant == "InstallComplete") {
    auto install_complete = dynamic_cast<event::InstallComplete &>(event);
    LOG_INFO << "ECU " << install_complete.serial << " installation has finished";
    pending_ecus--;
    if (pending_ecus == 0) {
      return std::make_shared<command::Shutdown>();
    } else {
      return nullptr;
    }
  } else if (event.variant == "Error") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_WARNING << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

std::shared_ptr<command::BaseCommand> EventsInterpreter::handle_campaign(event::BaseEvent &event) {
  if (event.variant == "CampaignCheckComplete" || event.variant == "CampaignAcceptComplete") {
    return std::make_shared<command::Shutdown>();
  }

  LOG_ERROR << "Unexpected event " << event.variant;
  return std::make_shared<command::Shutdown>();
}

void EventsInterpreter::run() {
  std::shared_ptr<event::BaseEvent> event;
  auto running_mode = config.uptane.running_mode;

  while (*events_channel >> event) {
    if (sig) {
      (*sig)(event);
    }
    if (event->variant == "DownloadProgressReport") {
      continue;
    }
    if (!sig) {
      LOG_INFO << "got " << event->variant << " event";
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
      case RunningMode::kCampaignAccept:
      case RunningMode::kCampaignReject:
        next_command = handle_campaign(*event);
        break;
      default:
        LOG_ERROR << "Unknown running mode " << StringFromRunningMode(running_mode);
    }

    if (next_command != nullptr) {
      *commands_channel << next_command;
    }
  }
}
