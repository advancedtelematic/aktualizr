#include <gtest/gtest.h>

#include <stdio.h>
#include <cstdlib>

#include <boost/make_shared.hpp>

#include "commands.h"
#include "config.h"
#include "dbusgateway/dbusgateway.h"
#include "events.h"
#include "types.h"

std::string fake_path;

TEST(SWMTest, DownloadComplete_method_called) {
  Config conf;

  command::Channel* chan = new command::Channel();

  DbusGateway gateway(conf, chan);
  data::DownloadComplete download_complete;
  download_complete.update_id = "testupdateid";
  download_complete.update_image = "/tmp/img.test";
  download_complete.signature = "signature";
  gateway.processEvent(boost::shared_ptr<event::BaseEvent>(new event::DownloadComplete(download_complete)));
  sleep(2);

  std::ifstream file_stream("/tmp/dbustestswm.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("DownloadComplete", content);
  file_stream.close();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    fake_path = argv[1];
    std::string cmd = fake_path + "swm.py &";
    EXPECT_EQ(0, system(cmd.c_str()));
    sleep(2);
  }
  return RUN_ALL_TESTS();
}
#endif
