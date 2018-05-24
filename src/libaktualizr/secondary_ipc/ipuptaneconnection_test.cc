/**
 * \file
 */

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <memory>
#include <utility>

#include "secondary_ipc/ipuptaneconnection.h"
#include "secondary_ipc/ipuptaneconnectionsplitter.h"
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

TEST(IpUptaneConnection, Splitter) {
  struct in6_addr secondary_addr_0;
  inet_pton(AF_INET6, "::ffff:127.0.0.1", &secondary_addr_0);

  IpUptaneConnection secondary_conn_0{0, secondary_addr_0};

  struct in6_addr secondary_addr_1;
  inet_pton(AF_INET6, "::ffff:127.0.0.2", &secondary_addr_1);
  IpUptaneConnection secondary_conn_1{0, secondary_addr_1};

  Uptane::SecondaryConfig secondary_conf_0;
  Uptane::SecondaryConfig secondary_conf_1;

  memset(&secondary_conf_0.ip_addr, 0, sizeof(sockaddr_storage));
  secondary_conf_0.ip_addr.ss_family = AF_INET6;
  reinterpret_cast<sockaddr_in6*>(&secondary_conf_0.ip_addr)->sin6_addr = secondary_addr_0;
  reinterpret_cast<sockaddr_in6*>(&secondary_conf_0.ip_addr)->sin6_port = htons(secondary_conn_0.listening_port());

  memset(&secondary_conf_1.ip_addr, 0, sizeof(sockaddr_storage));
  secondary_conf_1.ip_addr.ss_family = AF_INET6;
  reinterpret_cast<sockaddr_in6*>(&secondary_conf_1.ip_addr)->sin6_addr = secondary_addr_1;
  reinterpret_cast<sockaddr_in6*>(&secondary_conf_1.ip_addr)->sin6_port = htons(secondary_conn_1.listening_port());

  Uptane::IpUptaneSecondary secondary_0{secondary_conf_0};
  Uptane::IpUptaneSecondary secondary_1{secondary_conf_1};

  struct in6_addr primary_addr_in;
  inet_pton(AF_INET6, "::ffff:127.0.0.3", &primary_addr_in);
  IpUptaneConnection primary_conn{0, primary_addr_in};
  IpUptaneConnectionSplitter primary_splitter{primary_conn};

  primary_splitter.registerSecondary(secondary_0);
  primary_splitter.registerSecondary(secondary_1);

  sockaddr_storage primary_addr;
  memset(&primary_addr, 0, sizeof(sockaddr_storage));
  primary_addr.ss_family = AF_INET6;
  reinterpret_cast<sockaddr_in6*>(&primary_addr)->sin6_addr = primary_addr_in;
  reinterpret_cast<sockaddr_in6*>(&primary_addr)->sin6_port = htons(primary_conn.listening_port());

  std::string public_key1, private_key1, public_key2, private_key_2;
  Crypto::generateKeyPair(KeyType::kRSA2048, &public_key1, &private_key1);
  Crypto::generateKeyPair(KeyType::kRSA2048, &public_key2, &private_key_2);

  std::thread secondary_0_thread = std::thread([&]() {
    std::shared_ptr<SecondaryPacket> pack;
    while (secondary_conn_0.in_channel_ >> pack) {
      std::unique_ptr<SecondaryMessage> out_msg;
      if (pack->msg->mes_type == kSecondaryMesPublicKeyReqTag) {
        SecondaryPublicKeyResp* out_msg_pkey = new SecondaryPublicKeyResp;
        out_msg_pkey->type = kRSA2048;
        out_msg_pkey->key = public_key1;

        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_pkey};

        std::shared_ptr<SecondaryPacket> out_pack{new SecondaryPacket{primary_addr, std::move(out_msg)}};
        secondary_conn_0.out_channel_ << out_pack;
      }
    }
  });

  std::thread secondary_1_thread = std::thread([&]() {
    std::shared_ptr<SecondaryPacket> pack;
    while (secondary_conn_1.in_channel_ >> pack) {
      std::unique_ptr<SecondaryMessage> out_msg;
      if (pack->msg->mes_type == kSecondaryMesPublicKeyReqTag) {
        SecondaryPublicKeyResp* out_msg_pkey = new SecondaryPublicKeyResp;
        out_msg_pkey->type = kRSA2048;
        out_msg_pkey->key = public_key2;

        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_pkey};

        std::shared_ptr<SecondaryPacket> out_pack{new SecondaryPacket{primary_addr, std::move(out_msg)}};
        secondary_conn_1.out_channel_ << out_pack;
      }
    }
  });

  auto res_0 = secondary_0.getPublicKey();
  PublicKey pk1(public_key1, KeyType::kRSA2048);
  PublicKey pk2(public_key2, KeyType::kRSA2048);

  EXPECT_EQ(res_0, pk1);
  auto res_1 = secondary_1.getPublicKey();
  EXPECT_EQ(res_1, pk2);
  EXPECT_NE(res_1, res_0);

  secondary_conn_0.stop();
  secondary_conn_1.stop();
  primary_conn.stop();

  secondary_0_thread.join();
  secondary_1_thread.join();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif
