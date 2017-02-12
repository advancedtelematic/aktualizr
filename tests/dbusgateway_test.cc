#include <stdio.h>
#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include "commands.h"
#include "config.h"
#include "dbusgateway/dbusgateway.h"
#include "events.h"
#include "types.h"

std::string fake_path;

TEST(CommandsTest, initiateDownload_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  std::string cmd = "python " + fake_path + "dbus_send.py initiateDownload";
  system(cmd.c_str());
  boost::shared_ptr<command::BaseCommand> command;
  *chan >> command;

  EXPECT_EQ(command->variant, "StartDownload");
  command::StartDownload* start_download_command = static_cast<command::StartDownload*>(command.get());
  EXPECT_EQ(start_download_command->update_request_id, "testupdateid");
}

TEST(CommandsTest, abortDownload_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  std::string cmd = "python " + fake_path + "dbus_send.py abortDownload";
  system(cmd.c_str());
  boost::shared_ptr<command::BaseCommand> command;
  *chan >> command;

  EXPECT_EQ(command->variant, "AbortDownload");
  command::AbortDownload* abort_download_command = static_cast<command::AbortDownload*>(command.get());
  EXPECT_EQ(abort_download_command->update_request_id, "testupdateid");
}

TEST(CommandsTest, SendUpdateReport_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  std::string cmd = "python " + fake_path + "dbus_send.py updateReport";
  system(cmd.c_str());
  boost::shared_ptr<command::BaseCommand> command;
  *chan >> command;

  EXPECT_EQ(command->variant, "SendUpdateReport");
  command::SendUpdateReport* send_update_report_command = static_cast<command::SendUpdateReport*>(command.get());

  EXPECT_EQ(send_update_report_command->update_report.update_id, "testupdateid");
  EXPECT_EQ(send_update_report_command->update_report.operation_results.size(), 1);
  EXPECT_EQ(send_update_report_command->update_report.operation_results[0].id, "123");
  EXPECT_EQ(send_update_report_command->update_report.operation_results[0].result_code, 0);
  EXPECT_EQ(send_update_report_command->update_report.operation_results[0].result_text, "Good");
}

TEST(CommandsTest, DownloadCompleteSignal_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  data::DownloadComplete download_complete;
  download_complete.update_id = "testupdateid";
  download_complete.update_image = "/tmp/img.test";
  download_complete.signature = "signature";
  gateway.processEvent(boost::shared_ptr<event::BaseEvent>(new event::DownloadComplete(download_complete)));
  sleep(1);

  std::ifstream file_stream("/tmp/dbustestclient.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("DownloadComplete", content);
  file_stream.close();
}

TEST(CommandsTest, UpdateAvailableSignal_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  data::UpdateAvailable update_available;
  update_available.update_id = "testupdateid";
  update_available.description = "testdescription";
  update_available.signature = "signature";
  update_available.size = 200;
  gateway.fireUpdateAvailableEvent(update_available);
  gateway.processEvent(boost::shared_ptr<event::BaseEvent>(new event::UpdateAvailable(update_available)));

  sleep(1);

  std::ifstream file_stream("/tmp/dbustestclient.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("UpdateAvailable", content);
  file_stream.close();
}

TEST(CommandsTest, InstalledSoftwareNeededSignal_sent) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);

  gateway.processEvent(boost::shared_ptr<event::BaseEvent>(new event::InstalledSoftwareNeeded()));
  sleep(1);

  std::ifstream file_stream("/tmp/dbustestclient.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("InstalledSoftwareNeeded", content);
  file_stream.close();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    fake_path = argv[1];
    std::string cmd = "python " + fake_path + "dbus_recieve.py &";
    system(cmd.c_str());
    sleep(1);
  }
  return RUN_ALL_TESTS();
}
#endif
