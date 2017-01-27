#include "eventsinterpreter.h"
#include <unistd.h>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#ifdef WITH_DBUS
#include <CommonAPI/CommonAPI.hpp>
#include <v1/org/genivi/SoftwareLoadingManagerProxy.hpp>
#include "dbusgateway/dbusgateway.h"
#endif

#include "config.h"
#ifdef WITH_DBUS
using namespace v1::org::genivi;
#endif

EventsInterpreter::EventsInterpreter(const Config& config_in,
                                     event::Channel* events_channel_in,
                                     command::Channel* commands_channel_in)
    : config(config_in),
      events_channel(events_channel_in),
      commands_channel(commands_channel_in) {}

EventsInterpreter::~EventsInterpreter() { delete thread; }

void EventsInterpreter::interpret() {
  thread = new boost::thread(boost::bind(&EventsInterpreter::run, this));
}

void EventsInterpreter::run() {
#ifdef WITH_DBUS
  std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
  std::shared_ptr<SoftwareLoadingManagerProxy<> > swlm =
      runtime->buildProxy<SoftwareLoadingManagerProxy>(
          "local", config.dbus.software_manager);
  if (!swlm) {
    LOGGER_LOG(LVL_error, "DBUS session has not been found, exiting");
    exit(1);
  }
  std::shared_ptr<DbusGateway> dbus_gateway =
      std::make_shared<DbusGateway>(commands_channel);
  runtime->registerService("local", config.dbus.interface, dbus_gateway);

#endif

  boost::shared_ptr<event::BaseEvent> event;
  while (*events_channel >> event) {
    if (event->variant == "DownloadComplete") {
      LOGGER_LOG(LVL_info, "got DownloadComplete event");
#ifdef WITH_DBUS
      event::DownloadComplete* download_complete_event =
          static_cast<event::DownloadComplete*>(event.get());
      if (swlm->isAvailable()) {
        CommonAPI::CallStatus call_status;
        LOGGER_LOG(LVL_info,
                   "calling  downloadComplete dbus method of swlm service");
        swlm->downloadComplete(
            download_complete_event->download_complete.update_image,
            download_complete_event->download_complete.signature, call_status);
        if (call_status != CommonAPI::CallStatus::SUCCESS) {
          LOGGER_LOG(LVL_error,
                     "error of calling swlm.downloadComplete dbus method");
        }
      } else {
        LOGGER_LOG(LVL_info, "no " << config.dbus.software_manager
                                   << "dbus interface is running, skipping "
                                      "downloadComplete method call");
      }

      SotaClient::DownloadComplete download_complete_dbus;
      download_complete_dbus.setUpdate_id(
          download_complete_event->download_complete.update_id);
      download_complete_dbus.setUpdate_image(
          download_complete_event->download_complete.update_image);
      download_complete_dbus.setSignature(
          download_complete_event->download_complete.signature);
      dbus_gateway->fireDownloadCompleteEvent(download_complete_dbus);
#endif
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
#ifdef WITH_DBUS
    else if (event->variant == "InstalledSoftwareNeeded") {
      LOGGER_LOG(LVL_info, "got InstalledSoftwareNeeded event");
      dbus_gateway->fireInstalledSoftwareNeededEvent();
    } else if (event->variant == "UpdateAvailable") {
      LOGGER_LOG(LVL_info, "got UpdateAvailable event");
      event::UpdateAvailable* update_available =
          static_cast<event::UpdateAvailable*>(event.get());
      SotaClient::UpdateAvailable update_available_dbus;
      update_available_dbus.setUpdate_id(
          update_available->update_vailable.update_id);
      update_available_dbus.setDescription(
          update_available->update_vailable.description);
      update_available_dbus.setRequest_confirmation(
          update_available->update_vailable.request_confirmation);
      update_available_dbus.setSignature(
          update_available->update_vailable.signature);
      update_available_dbus.setSize(update_available->update_vailable.size);
      dbus_gateway->fireUpdateAvailableEvent(update_available_dbus);
    } else if (event->variant == "DownloadComplete") {
      LOGGER_LOG(LVL_info, "got DownloadComplete event");
      event::DownloadComplete* download_complete =
          static_cast<event::DownloadComplete*>(event.get());
      SotaClient::DownloadComplete download_complete_dbus;
      download_complete_dbus.setUpdate_id(
          download_complete->download_complete.update_id);
      download_complete_dbus.setUpdate_image(
          download_complete->download_complete.update_image);
      download_complete_dbus.setSignature(
          download_complete->download_complete.signature);
      dbus_gateway->fireDownloadCompleteEvent(download_complete_dbus);
    }
#endif
  }
}
