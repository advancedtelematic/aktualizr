#define BOOST_TEST_MODULE test_dbusgateway

#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <string>
#include <boost/shared_ptr.hpp>


#include "dbusgateway/dbusgateway.h"
#include <CommonAPI/CommonAPI.hpp>
#include "commands.h"
#include "events.h"
using namespace v1::org::genivi::swm;


BOOST_AUTO_TEST_CASE(test_dbus_commands) {
    event::Channel e_chan;
    command::Channel c_chan;
    DbusGateway g(&c_chan, &e_chan);

    std::shared_ptr<CommonAPI::ClientId>client;
    SotaClientStub::abortDownloadReply_t callback = []() { };
    g.abortDownload(client, "update_request_id123", callback);
    if (c_chan.hasValues()){
        boost::shared_ptr<command::BaseCommand> abort_download;
        c_chan >> abort_download;
        BOOST_CHECK_EQUAL(abort_download->variant, "AbortDownload");
        BOOST_CHECK_EQUAL(static_cast<command::AbortDownload*>(abort_download.get())->update_request_id, "update_request_id123");
    }
    else{
        BOOST_FAIL("channel should not be empty");
    }

    SotaClientStub::initiateDownloadReply_t callback2 = []() { };
    g.initiateDownload(client, "update_request_id123", callback2);
    if (c_chan.hasValues()){
        boost::shared_ptr<command::BaseCommand> start_download;
        c_chan >> start_download;
        BOOST_CHECK_EQUAL(start_download->variant, "StartDownload");
        BOOST_CHECK_EQUAL(static_cast<command::StartDownload*>(start_download.get())->update_request_id, "update_request_id123");
    }
    else{
        BOOST_FAIL("channel should not be empty");
    }

    SotaClient::OperationResult operation_result;
    operation_result.setId("id123");
    operation_result.setResultCode(SotaClient::SWMResult::SWM_RES_FLASH_FAILED);
    operation_result.setResultText("failed");

    std::vector<SotaClient::OperationResult> operation_results;
    operation_results.push_back(operation_result);
    SotaClientStub::updateReportReply_t callback3 = []() { };
    g.updateReport(client, "updateid", operation_results, callback3);
    if (c_chan.hasValues()){
        boost::shared_ptr<command::BaseCommand> send_update_report;
        c_chan >> send_update_report;
        BOOST_CHECK_EQUAL(send_update_report->variant, "SendUpdateReport");
        BOOST_CHECK_EQUAL(static_cast<command::SendUpdateReport*>(send_update_report.get())->update_report.update_id, "updateid");
        BOOST_CHECK_EQUAL(static_cast<command::SendUpdateReport*>(send_update_report.get())->update_report.operation_results[0].id, "id123");
        BOOST_CHECK_EQUAL(static_cast<command::SendUpdateReport*>(send_update_report.get())->update_report.operation_results[0].result_code, data::UpdateResultCode::FLASH_FAILED);
        BOOST_CHECK_EQUAL(static_cast<command::SendUpdateReport*>(send_update_report.get())->update_report.operation_results[0].result_text, "failed");

    }
    else{
        BOOST_FAIL("channel should not be empty");
    }
}


