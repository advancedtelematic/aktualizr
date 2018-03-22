/**
 * \file
 */

#include <gtest/gtest.h>

#include <memory>

#include "ipuptaneconnection.h"
#include "ipuptaneconnectionsplitter.h"

#include "logging.h"

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
  IpUptaneConnection sender{0};
  IpUptaneConnection receiver{0};

  EXPECT_FALSE(sender.in_channel_.hasValues());
  EXPECT_FALSE(sender.out_channel_.hasValues());
  EXPECT_FALSE(receiver.in_channel_.hasValues());
  EXPECT_FALSE(receiver.out_channel_.hasValues());

  EXPECT_TRUE(
      test_message<SecondaryPublicKeyReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryPublicKeyResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryManifestReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryManifestResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryPutMetaReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryPutMetaResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryRootVersionReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryRootVersionResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryPutRootReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondaryPutRootResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondarySendFirmwareReq>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
  EXPECT_TRUE(
      test_message<SecondarySendFirmwareResp>(receiver.listening_address(), sender.out_channel_, receiver.in_channel_));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif
