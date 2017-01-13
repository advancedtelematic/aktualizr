#include "dbusgateway.h"

#include <boost/shared_ptr.hpp>

#include "types.h"

using namespace v1::org::genivi::swm;

DbusGateway::DbusGateway(command::Channel *command_channel_in, event::Channel *event_channel_in):command_channel(command_channel_in), event_channel(event_channel_in) { }
DbusGateway::~DbusGateway() { }

void DbusGateway::run(){
     thread = new std::thread(std::bind(&DbusGateway::process_events, this));
}

void DbusGateway::process_events(){
    boost::shared_ptr<event::BaseEvent> event;
    while (*event_channel >> event) {
      if (event->variant == "InstalledSoftwareNeeded") {
        fireInstalledSoftwareNeededEvent();

      }else if (event->variant == "UpdateAvailable"){

        event::UpdateAvailable* update_available =  static_cast<event::UpdateAvailable*>(event.get());
        SotaClient::UpdateAvailable update_available_dbus;
        update_available_dbus.setUpdate_id(update_available->update_vailable.update_id);
        update_available_dbus.setDescription(update_available->update_vailable.description);
        update_available_dbus.setRequest_confirmation(update_available->update_vailable.request_confirmation);
        update_available_dbus.setSignature(update_available->update_vailable.signature);
        update_available_dbus.setSize(update_available->update_vailable.size);
        fireUpdateAvailableEvent(update_available_dbus);
      }else if (event->variant == "DownloadComplete"){

        event::DownloadComplete* download_complete =  static_cast<event::DownloadComplete*>(event.get());
        SotaClient::DownloadComplete download_complete_dbus;
        download_complete_dbus.setUpdate_id(download_complete->download_complete.update_id);
        download_complete_dbus.setUpdate_image(download_complete->download_complete.update_image);
        download_complete_dbus.setSignature(download_complete->download_complete.signature);
        fireDownloadCompleteEvent(download_complete_dbus);
      }
    }
}


/**
 * description: Sent by SC to start the download of an update previously announced
    as
 *   available through an update_available() call made from SC to
    SWLM.
 */
void DbusGateway::initiateDownload(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _updateId, initiateDownloadReply_t _reply) {
    (void)_client;
    *command_channel <<  boost::shared_ptr<command::StartDownload>(new command::StartDownload(_updateId));
    _reply();
}

/**
 * description: Abort a download previously initiated with initiate_download().
    Invoked by
 *   SWLM in response to an error or an explicit
    request sent by HMI to SWLM in
 *   response to a user abort.
 */
void DbusGateway::abortDownload(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _updateId, abortDownloadReply_t _reply) {
    (void)_client;
    *command_channel <<  boost::shared_ptr<command::AbortDownload>(new command::AbortDownload(_updateId));

    _reply();
}

/**
 * description: Receive an update report from SWLM with the processing result of all
    bundled
 *   operations.
    An update report message can either be sent in response
    to an
 *   downloadComplete() message transmitted from SC to SWLM,
    or be sent
 *   unsolicited by SWLM to SC
 */
void DbusGateway::updateReport(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _updateId, std::vector<SotaClient::OperationResult> _operationsResults, updateReportReply_t _reply) {
    (void)_client;
    data::UpdateReport update_report;
    for(SotaClient::OperationResult dbus_operation_result : _operationsResults){
        data::OperationResult operation_result;
        operation_result.id = dbus_operation_result.getId();
        operation_result.result_code = static_cast<data::UpdateResultCode>(static_cast<int32_t>(dbus_operation_result.getResultCode()));
        operation_result.result_text = dbus_operation_result.getResultText();
        update_report.operation_results.push_back(operation_result);
    }
    update_report.update_id = _updateId;
    *command_channel << boost::shared_ptr<command::SendUpdateReport>(new command::SendUpdateReport(update_report));


    _reply();
}

