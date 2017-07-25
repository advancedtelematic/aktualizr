#include <stdio.h>
#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include "commands.h"
#include "config.h"
#include "events.h"
#include "socketgateway.h"
#include "types.h"

const std::string server = "http://127.0.0.1:8800";
std::string fake_path;

TEST(EventsTest, broadcasted) {
  NetworkConfig network_conf;
  network_conf.socket_commands_path = "/tmp/sota-commands.socket";
  network_conf.socket_events_path = "/tmp/sota-events.socket";
  network_conf.socket_events.push_back("InstalledSoftwareNeeded");
  Config conf;
  conf.network = network_conf;

  command::Channel* chan = new command::Channel();

  SocketGateway gateway(conf, chan);
  sleep(1);
  std::string cmd = "python " + fake_path + "events.py &";
  EXPECT_EQ(system(cmd.c_str()), 0);
  sleep(1);
  gateway.processEvent(boost::make_shared<event::InstalledSoftwareNeeded>(event::InstalledSoftwareNeeded()));
  sleep(1);
  std::ifstream file_stream("/tmp/sota-events.socket.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("{\"fields\":[],\"variant\":\"InstalledSoftwareNeeded\"}", content);
}

TEST(EventsTest, not_broadcasted) {
  NetworkConfig network_conf;
  network_conf.socket_commands_path = "/tmp/sota-commands.socket";
  network_conf.socket_events_path = "/tmp/sota-events.socket";
  network_conf.socket_events.empty();
  Config conf;
  conf.network = network_conf;

  command::Channel* chan = new command::Channel();

  SocketGateway gateway(conf, chan);
  std::string cmd = "python " + fake_path + "events.py &";
  EXPECT_EQ(system(cmd.c_str()), 0);
  sleep(1);
  gateway.processEvent(boost::make_shared<event::InstalledSoftwareNeeded>(event::InstalledSoftwareNeeded()));
  sleep(1);
  std::ifstream file_stream("/tmp/sota-events.socket.txt");
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ("", content);
}

TEST(CommandsTest, recieved) {
  NetworkConfig network_conf;
  network_conf.socket_commands_path = "/tmp/sota-commands.socket";
  network_conf.socket_events_path = "/tmp/sota-events.socket";
  Config conf;
  conf.network = network_conf;

  command::Channel* chan = new command::Channel();

  SocketGateway gateway(conf, chan);
  std::string cmd = "python " + fake_path + "commands.py &";
  EXPECT_EQ(system(cmd.c_str()), 0);
  sleep(1);
  boost::shared_ptr<command::BaseCommand> command;
  *chan >> command;

  EXPECT_EQ(command->variant, "Shutdown");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    fake_path = argv[1];
  }
  return RUN_ALL_TESTS();
}
#endif
