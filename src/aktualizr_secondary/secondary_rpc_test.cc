#include <gtest/gtest.h>

#include <netinet/tcp.h>

#include "aktualizr_secondary_interface.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "msg_dispatcher.h"
#include "secondary_tcp_server.h"
#include "storage/invstorage.h"
#include "test_utils.h"

class SecondaryMock : public IAktualizrSecondary {
 public:
  SecondaryMock(const Uptane::EcuSerial& serial, const Uptane::HardwareIdentifier& hdw_id, const PublicKey& pub_key,
                const Uptane::Manifest& manifest)
      : serial_(serial),
        hdw_id_(hdw_id),
        pub_key_(pub_key),
        manifest_(manifest),
        msg_dispatcher_{*this},
        image_filepath_{image_dir_ / "image.bin"},
        hasher_{MultiPartHasher::create(Hash::Type::kSha256)} {}

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
    (void)data;
    return true;
  }

  data::ResultCode::Numeric sendFirmware(const uint8_t* data, size_t size) override {
    std::ofstream target_file(image_filepath_.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);

    target_file.write(reinterpret_cast<const char*>(data), size);
    hasher_->update(data, size);

    target_file.close();
    return data::ResultCode::Numeric::kOk;
  }

  data::ResultCode::Numeric install(const std::string& target_name) override {
    (void)target_name;
    return data::ResultCode::Numeric::kOk;
  }

  Hash getReceivedImageHash() const { return hasher_->getHash(); }

  size_t getReceivedImageSize() const { return boost::filesystem::file_size(image_filepath_); }

 public:
  const Uptane::EcuSerial serial_;
  const Uptane::HardwareIdentifier hdw_id_;
  const PublicKey pub_key_;
  const Uptane::Manifest manifest_;

  Uptane::RawMetaPack metapack_;

 private:
  AktualizrSecondaryMsgDispatcher msg_dispatcher_;
  TemporaryDirectory image_dir_;
  boost::filesystem::path image_filepath_;
  std::shared_ptr<MultiPartHasher> hasher_;
};

bool operator==(const Uptane::RawMetaPack& lhs, const Uptane::RawMetaPack& rhs) {
  return (lhs.director_root == rhs.director_root) && (lhs.image_root == rhs.image_root) &&
         (lhs.director_targets == rhs.director_targets) && (lhs.image_snapshot == rhs.image_snapshot) &&
         (lhs.image_timestamp == rhs.image_timestamp) && (lhs.image_targets == rhs.image_targets);
}

class TargetFile {
 public:
  TargetFile(const std::string filename, size_t size = 1024, Hash::Type hash_type = Hash::Type::kSha256)
      : image_size_{size},
        image_filepath_{target_dir_ / filename},
        image_hash_{generateRandomFile(image_filepath_, size, hash_type)} {}

  std::string path() const { return image_filepath_.string(); }
  const Hash& hash() const { return image_hash_; }
  const size_t& size() const { return image_size_; }

  static ImageReader getImageReader() {
    return [](const Uptane::Target& target) { return std_::make_unique<ImageReaderMock>(target.filename()); };
  }

 private:
  class ImageReaderMock : public StorageTargetRHandle {
   public:
    ImageReaderMock(boost::filesystem::path image_filename) : image_filename_{image_filename} {
      image_file_.open(image_filename_);
    }

    ~ImageReaderMock() override {
      if (image_file_.is_open()) {
        image_file_.close();
      }
    }

    bool isPartial() const override { return false; }
    std::unique_ptr<StorageTargetWHandle> toWriteHandle() override { return nullptr; }

    uintmax_t rsize() const override { return boost::filesystem::file_size(image_filename_); }

    size_t rread(uint8_t* buf, size_t size) override {
      return image_file_.readsome(reinterpret_cast<char*>(buf), size);
    }

    void rclose() override { image_file_.close(); }

   private:
    boost::filesystem::path image_filename_;
    boost::filesystem::ifstream image_file_;
  };

  static Hash generateRandomFile(const boost::filesystem::path& filepath, size_t size, Hash::Type hash_type) {
    auto hasher = MultiPartHasher::create(hash_type);
    std::ofstream file{filepath.string(), std::ofstream::binary};

    if (!file.is_open() || !file.good()) {
      throw std::runtime_error("Failed to create a file: " + filepath.string());
    }

    const unsigned char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv";
    unsigned char cur_symbol;

    for (unsigned int ii = 0; ii < size; ++ii) {
      cur_symbol = symbols[rand() % sizeof(symbols)];
      file.put(cur_symbol);
      hasher->update(&cur_symbol, sizeof(cur_symbol));
    }

    file.close();
    return hasher->getHash();
  }

 private:
  TemporaryDirectory target_dir_;
  const size_t image_size_;
  boost::filesystem::path image_filepath_;
  Hash image_hash_;
};

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

  TargetFile target_file("mytarget_image.img", 1024 * 3 + 1);
  Uptane::SecondaryInterface::Ptr ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate(
      "localhost", secondary_server.port(), target_file.getImageReader(), nullptr);

  ASSERT_TRUE(ip_secondary != nullptr) << "Failed to create IP Secondary";
  EXPECT_EQ(ip_secondary->getSerial(), secondary.getSerial());
  EXPECT_EQ(ip_secondary->getHwId(), secondary.getHwId());
  EXPECT_EQ(ip_secondary->getPublicKey(), secondary.getPublicKey());
  EXPECT_EQ(ip_secondary->getManifest(), secondary.getManifest());

  Uptane::RawMetaPack meta_pack{"director-root", "director-target", "image_root",
                                "image_targets", "image_timestamp", "image_snapshot"};

  EXPECT_TRUE(ip_secondary->putMetadata(meta_pack));
  EXPECT_TRUE(meta_pack == secondary.metapack_);
  Json::Value target_json;
  target_json["custom"]["targetFormat"] = "BINARY";
  EXPECT_EQ(ip_secondary->install(Uptane::Target(target_file.path(), target_json)), data::ResultCode::Numeric::kOk);

  EXPECT_EQ(target_file.hash(), secondary.getReceivedImageHash());

  secondary_server.stop();
  secondary_server_thread.join();
}

class ImageTransferTest : public ::testing::Test, public ::testing::WithParamInterface<size_t> {
 protected:
  ImageTransferTest()
      : secondary_{Uptane::EcuSerial("serial"), Uptane::HardwareIdentifier("hardware-id"),
                   PublicKey("pub-key", KeyType::kED25519), Uptane::Manifest()},
        secondary_server_{secondary_.getDispatcher(), "", 0},
        secondary_server_thread_{std::bind(&ImageTransferTest::run_secondary_server, this)},
        image_file_{"mytarget_image.img", GetParam()} {
    secondary_server_.wait_until_running();
    ip_secondary_ = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_server_.port(),
                                                                image_file_.getImageReader(), nullptr);
  }

  ~ImageTransferTest() {
    secondary_server_.stop();
    secondary_server_thread_.join();
  }

  void run_secondary_server() { secondary_server_.run(); }

  data::ResultCode::Numeric sendAndInstallImage() {
    Json::Value target_json;
    target_json["custom"]["targetFormat"] = "BINARY";
    return ip_secondary_->install(Uptane::Target(image_file_.path(), target_json));
  }

 protected:
  SecondaryMock secondary_;
  SecondaryTcpServer secondary_server_;
  std::thread secondary_server_thread_;
  TargetFile image_file_;
  Uptane::SecondaryInterface::Ptr ip_secondary_;
};

TEST_P(ImageTransferTest, ImageTransferEdgeCases) {
  ASSERT_TRUE(ip_secondary_ != nullptr) << "Failed to create IP Secondary";

  EXPECT_EQ(sendAndInstallImage(), data::ResultCode::Numeric::kOk);
  EXPECT_EQ(image_file_.hash(), secondary_.getReceivedImageHash());
  EXPECT_EQ(image_file_.size(), secondary_.getReceivedImageSize());
}

INSTANTIATE_TEST_SUITE_P(ImageTransferTestEdgeCases, ImageTransferTest,
                         ::testing::Values(1, 1024, 1024 - 1, 1024 + 1, 1024 * 10 + 1, 1024 * 10 - 1));

TEST(SecondaryTcpServer, TestIpSecondaryIfSecondaryIsNotRunning) {
  in_port_t secondary_port = TestUtils::getFreePortAsInt();
  Uptane::SecondaryInterface::Ptr ip_secondary;

  // trying to connect to a non-running Secondary and create a corresponding instance on Primary
  ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_port, nullptr, nullptr);
  EXPECT_EQ(ip_secondary, nullptr);

  TargetFile target_file("mytarget_image.img");
  // create Primary's secondary without connecting to Secondary
  ip_secondary = std::make_shared<Uptane::IpUptaneSecondary>(
      "localhost", secondary_port, Uptane::EcuSerial("serial"), Uptane::HardwareIdentifier("hwid"),
      PublicKey("key", KeyType::kED25519), target_file.getImageReader(), nullptr);

  Uptane::RawMetaPack meta_pack{"director-root", "director-target", "image_root",
                                "image_targets", "image_timestamp", "image_snapshot"};

  // expect failures since the secondary is not running
  EXPECT_EQ(ip_secondary->getManifest(), Json::Value());
  EXPECT_FALSE(ip_secondary->putMetadata(meta_pack));
  Json::Value target_json;
  target_json["custom"]["targetFormat"] = "BINARY";
  EXPECT_EQ(ip_secondary->install(Uptane::Target(target_file.path(), target_json)),
            data::ResultCode::Numeric::kInstallFailed);
}

class SecondaryRpcTestNegative : public ::testing::Test {
 protected:
  SecondaryRpcTestNegative()
      : secondary_server_{msg_dispatcher_, "", 0}, secondary_server_thread_{[&]() { secondary_server_.run(); }} {
    msg_dispatcher_.registerHandler(AKIpUptaneMes_PR_installReq, [](Asn1Message& in_msg, Asn1Message& out_msg) {
      (void)in_msg;
      out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result = AKInstallationResultCode_ok;
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
