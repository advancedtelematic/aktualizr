#include <gtest/gtest.h>

#include <netinet/tcp.h>

#include "aktualizr_secondary_interface.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "msg_dispatcher.h"
#include "secondary_tcp_server.h"
#include "test_utils.h"

class SecondaryMock : public IAktualizrSecondary {
 public:
  SecondaryMock(const Uptane::EcuSerial& serial, const Uptane::HardwareIdentifier& hdw_id, const PublicKey& pub_key,
                const Uptane::Manifest& manifest)
      : serial_(serial), hdw_id_(hdw_id), pub_key_(pub_key), manifest_(manifest), msg_dispatcher_{*this} {}

 public:
  MsgDispatcher& getDispatcher() { return msg_dispatcher_; }
  Uptane::EcuSerial getSerial() const { return serial_; }
  Uptane::HardwareIdentifier getHwId() const { return hdw_id_; }
  PublicKey getPublicKey() const { return pub_key_; }

  std::tuple<Uptane::EcuSerial, Uptane::HardwareIdentifier, PublicKey> getInfo() const override {
    return {serial_, hdw_id_, pub_key_};
  }

  Uptane::Manifest getManifest() const override { return manifest_; }

  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override {
    metapack_ = meta_pack;
    return true;
  }

  bool sendFirmware(const std::string& data) override {
    data_ = data;
    return true;
  }

  data::ResultCode::Numeric install(const std::string& target_name) override {
    (void)target_name;
    return data::ResultCode::Numeric::kOk;
  }

 public:
  const Uptane::EcuSerial serial_;
  const Uptane::HardwareIdentifier hdw_id_;
  const PublicKey pub_key_;
  const Uptane::Manifest manifest_;

  Uptane::RawMetaPack metapack_;
  std::string data_;

 private:
  AktualizrSecondaryMsgDispatcher msg_dispatcher_;
};

bool operator==(const Uptane::RawMetaPack& lhs, const Uptane::RawMetaPack& rhs) {
  return (lhs.director_root == rhs.director_root) && (lhs.image_root == rhs.image_root) &&
         (lhs.director_targets == rhs.director_targets) && (lhs.image_snapshot == rhs.image_snapshot) &&
         (lhs.image_timestamp == rhs.image_timestamp) && (lhs.image_targets == rhs.image_targets);
}

// Test the serialization/deserialization and the TCP/IP communication implementation
// that occurs during communication between Primary and IP Secondary
TEST(SecondaryTcpServer, TestIpSecondaryRPC) {
  // secondary object on Secondary ECU
  SecondaryMock secondary(Uptane::EcuSerial("serial"), Uptane::HardwareIdentifier("hardware-id"),
                          PublicKey("pub-key", KeyType::kED25519), Uptane::Manifest());

  // create Secondary on Secondary ECU, and run it in a dedicated thread
  SecondaryTcpServer secondary_server(secondary.getDispatcher(), "", 0);
  std::thread secondary_server_thread{[&secondary_server]() { secondary_server.run(); }};

  secondary_server.wait_until_running();
  // create Secondary on Primary ECU, try it a few times since the secondary thread
  // might not be ready at the moment of the first try
  Uptane::SecondaryInterface::Ptr ip_secondary =
      Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_server.port());

  ASSERT_TRUE(ip_secondary != nullptr) << "Failed to create IP Secondary";
  EXPECT_EQ(ip_secondary->getSerial(), secondary.getSerial());
  EXPECT_EQ(ip_secondary->getHwId(), secondary.getHwId());
  EXPECT_EQ(ip_secondary->getPublicKey(), secondary.getPublicKey());
  EXPECT_EQ(ip_secondary->getManifest(), secondary.getManifest());

  Uptane::RawMetaPack meta_pack{"director-root", "director-target", "image_root",
                                "image_targets", "image_timestamp", "image_snapshot"};

  EXPECT_TRUE(ip_secondary->putMetadata(meta_pack));
  EXPECT_TRUE(meta_pack == secondary.metapack_);

  std::string firmware = "firmware";
  EXPECT_TRUE(ip_secondary->sendFirmware(firmware));
  EXPECT_EQ(firmware, secondary.data_);

  EXPECT_EQ(ip_secondary->install(""), data::ResultCode::Numeric::kOk);

  secondary_server.stop();
  secondary_server_thread.join();
}

TEST(SecondaryTcpServer, TestIpSecondaryIfSecondaryIsNotRunning) {
  in_port_t secondary_port = TestUtils::getFreePortAsInt();
  Uptane::SecondaryInterface::Ptr ip_secondary;

  // trying to connect to a non-running Secondary and create a corresponding instance on Primary
  ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_port);
  EXPECT_EQ(ip_secondary, nullptr);

  // create Primary's secondary without connecting to Secondary
  ip_secondary = std::make_shared<Uptane::IpUptaneSecondary>("localhost", secondary_port, Uptane::EcuSerial("serial"),
                                                             Uptane::HardwareIdentifier("hwid"),
                                                             PublicKey("key", KeyType::kED25519));

  Uptane::RawMetaPack meta_pack{"director-root", "director-target", "image_root",
                                "image_targets", "image_timestamp", "image_snapshot"};

  // expect failures since the secondary is not running
  EXPECT_EQ(ip_secondary->getManifest(), Json::Value());
  EXPECT_FALSE(ip_secondary->sendFirmware("firmware"));
  EXPECT_FALSE(ip_secondary->putMetadata(meta_pack));
  EXPECT_EQ(ip_secondary->install(""), data::ResultCode::Numeric::kInternalError);
}

class SecondaryRpcTestNegative : public ::testing::Test {
 protected:
  SecondaryRpcTestNegative()
      : secondary_server_{msg_dispatcher_, "", 0}, secondary_server_thread_{[&]() { secondary_server_.run(); }} {
    msg_dispatcher_.registerHandler(AKIpUptaneMes_PR_installReq, [](Asn1Message& in_msg, Asn1Message& out_msg) {
      (void)in_msg;
      out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result = AKInstallationResultCode_ok;
      // out_msg.installResp()->result = AKInstallationResultCode_ok;
      return MsgDispatcher::HandleStatusCode::kOk;
    });
    secondary_server_.wait_until_running();
  }

  ~SecondaryRpcTestNegative() {
    secondary_server_.stop();
    secondary_server_thread_.join();
  }

  AKIpUptaneMes_PR sendInstallMsg() {
    // compose and send a valid message
    Asn1Message::Ptr req(Asn1Message::Empty());
    req->present(AKIpUptaneMes_PR_installReq);

    // prepare request message
    auto req_mes = req->installReq();
    SetString(&req_mes->hash, "target_name");
    // send request and receive response, a request-response type of RPC
    std::pair<std::string, uint16_t> secondary_server_addr{"127.0.0.1", secondary_server_.port()};
    auto resp = Asn1Rpc(req, secondary_server_addr);

    return resp->present();
  }

 protected:
  MsgDispatcher msg_dispatcher_;
  SecondaryTcpServer secondary_server_;
  std::thread secondary_server_thread_;
};

// The given test fails because the Secondary TCP server implementation
// is a single-threaded, synchronous and blocking hence it cannot accept any new connections
// until the current one is closed. Therefore, if a client/Primary does not close its socket for some reason then
// Secondary becomes "unavailable"
// TEST_F(SecondaryRpcTestNegative, primaryNotClosingSocket) {
//  ConnectionSocket con_sock{"127.0.0.1", secondary_server_.port()};
//  con_sock.connect();
//  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
//}

TEST_F(SecondaryRpcTestNegative, primaryConnectAndDisconnect) {
  ConnectionSocket{"127.0.0.1", secondary_server_.port()}.connect();
  // do a valid request/response exchange to verify if Secondary works as expected
  // after accepting and closing a new connection
  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
}

TEST_F(SecondaryRpcTestNegative, primaryConnectAndSendValidButNotSupportedMsg) {
  {
    ConnectionSocket con_sock{"127.0.0.1", secondary_server_.port()};
    con_sock.connect();
    uint8_t garbage[] = {0x30, 0x13, 0x02, 0x01, 0x05, 0x16, 0x0e, 0x41, 0x6e, 0x79, 0x62,
                         0x6f, 0x64, 0x79, 0x20, 0x74, 0x68, 0x65, 0x72, 0x65, 0x3f};
    send(*con_sock, garbage, sizeof(garbage), 0);
  }
  // do a valid request/response exchange to verify if Secondary works as expected
  // after receiving not-supported message
  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
}

TEST_F(SecondaryRpcTestNegative, primaryConnectAndSendBrokenAsn1Msg) {
  {
    ConnectionSocket con_sock{"127.0.0.1", secondary_server_.port()};
    con_sock.connect();
    uint8_t garbage[] = {0x30, 0x99, 0x02, 0x01, 0x05, 0x16, 0x0e, 0x41, 0x6e, 0x79,
                         0x62, 0x6f, 0x64, 0x79, 0x20, 0x74, 0x68, 0x72, 0x65, 0x3f};
    send(*con_sock, garbage, sizeof(garbage), 0);
  }
  // do a valid request/response exchange to verify if Secondary works as expected
  // after receiving broken ASN1 message
  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
}

TEST_F(SecondaryRpcTestNegative, primaryConnectAndSendGarbage) {
  {
    ConnectionSocket con_sock{"127.0.0.1", secondary_server_.port()};
    con_sock.connect();
    uint8_t garbage[] = "some garbage message";
    send(*con_sock, garbage, sizeof(garbage), 0);
  }
  // do a valid request/response exchange to verify if Secondary works as expected
  // after receiving some garbage
  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::debug);

  return RUN_ALL_TESTS();
}
