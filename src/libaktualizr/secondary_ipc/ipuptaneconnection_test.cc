/**
 * \file
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <memory>
#include <utility>

#include "secondary_ipc/ipuptaneconnection.h"
#include "uptane/ipuptanesecondary.h"
#include "uptane/secondaryconfig.h"

#include "logging/logging.h"

template <typename Mes>
bool test_message(sockaddr_storage addr, SecondaryPacket::ChanType& out_channel,
                  SecondaryPacket::ChanType& in_channel) {
  std::unique_ptr<SecondaryMessage> msg = std::unique_ptr<SecondaryMessage>{new Mes};
  std::shared_ptr<SecondaryPacket> out_pack =
      std::shared_ptr<SecondaryPacket>{new SecondaryPacket{addr, std::move(msg)}};

  out_channel << out_pack;

  std::shared_ptr<SecondaryPacket> in_pack;
  in_channel.setTimeout(std::chrono::milliseconds(1000));
  return (in_channel >> in_pack) && (*in_pack->msg == *out_pack->msg);
}

TEST(IpUptaneConnection, SendRecv) {
  struct in6_addr receiver_addr;
  inet_pton(AF_INET6, "::ffff:127.0.0.1", &receiver_addr);
  IpUptaneConnection sender{0, receiver_addr};
  IpUptaneConnection receiver{0};

  EXPECT_FALSE(sender.in_channel_.hasValues());
  EXPECT_FALSE(sender.out_channel_.hasValues());
  EXPECT_FALSE(receiver.in_channel_.hasValues());
  EXPECT_FALSE(receiver.out_channel_.hasValues());

  // for this to run in Docker IPv4 should be used
  sockaddr_storage addr;
  memset(&addr, 0, sizeof(sockaddr_storage));
  addr.ss_family = AF_INET6;
  reinterpret_cast<sockaddr_in6*>(&addr)->sin6_addr = receiver_addr;
  reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port = htons(receiver.listening_port());

  EXPECT_TRUE(test_message<SecondaryPublicKeyReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryPublicKeyResp>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryManifestReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryManifestResp>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryPutMetaReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryPutMetaResp>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryRootVersionReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryRootVersionResp>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryPutRootReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondaryPutRootResp>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondarySendFirmwareReq>(addr, sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(test_message<SecondarySendFirmwareResp>(addr, sender.out_channel_, receiver.in_channel_));
  sender.stop();
  receiver.stop();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif
