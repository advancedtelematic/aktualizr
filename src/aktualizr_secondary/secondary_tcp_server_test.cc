#include <gtest/gtest.h>

#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "secondary_tcp_server.h"
#include "test_utils.h"
#include "uptane/secondaryinterface.h"

class SecondaryMock : public Uptane::SecondaryInterface {
 public:
  SecondaryMock(const Uptane::EcuSerial& serial, const Uptane::HardwareIdentifier& hdw_id, const PublicKey& pub_key,
                const Uptane::Manifest& manifest)
      : _serial(serial), _hdw_id(hdw_id), _pub_key(pub_key), _manifest(manifest) {}

 public:
  virtual std::string Type() const { return "mock"; }
  virtual Uptane::EcuSerial getSerial() const { return _serial; }
  virtual Uptane::HardwareIdentifier getHwId() const { return _hdw_id; }
  virtual PublicKey getPublicKey() const { return _pub_key; }
  virtual Uptane::Manifest getManifest() const { return _manifest; }
  virtual bool putMetadata(const Uptane::RawMetaPack& meta_pack) {
    _metapack = meta_pack;
    return true;
  }
  virtual int32_t getRootVersion(bool director) const {
    (void)director;
    return 0;
  }
  virtual bool putRoot(const std::string& root, bool director) {
    (void)root;
    (void)director;
    return true;
  }

  virtual bool sendFirmware(const std::string& data) {
    _data = data;
    return true;
  }

  virtual data::ResultCode::Numeric install(const std::string& target_name) {
    (void)target_name;
    return data::ResultCode::Numeric::kOk;
  }

 public:
  const Uptane::EcuSerial _serial;
  const Uptane::HardwareIdentifier _hdw_id;
  const PublicKey _pub_key;
  const Uptane::Manifest _manifest;

  Uptane::RawMetaPack _metapack;
  std::string _data;
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
  SecondaryTcpServer secondary_server{secondary};
  std::thread secondary_server_thread{[&secondary_server]() { secondary_server.run(); }};

  // create Secondary on Primary ECU, try it a few times since the secondary thread
  // might not be ready at the moment of the first try
  const int max_try = 5;
  Uptane::SecondaryInterface::Ptr ip_secondary;
  for (int ii = 0; ii < max_try && ip_secondary == nullptr; ++ii) {
    ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_server.port());
  }

  ASSERT_TRUE(ip_secondary != nullptr) << "Failed to create IP Secondary";
  EXPECT_EQ(ip_secondary->getSerial(), secondary.getSerial());
  EXPECT_EQ(ip_secondary->getHwId(), secondary.getHwId());
  EXPECT_EQ(ip_secondary->getPublicKey(), secondary.getPublicKey());
  EXPECT_EQ(ip_secondary->getManifest(), secondary.getManifest());

  Uptane::RawMetaPack meta_pack{"director-root", "director-target", "image_root",
                                "image_targets", "image_timestamp", "image_snapshot"};

  EXPECT_TRUE(ip_secondary->putMetadata(meta_pack));
  EXPECT_TRUE(meta_pack == secondary._metapack);

  std::string firmware = "firmware";
  EXPECT_TRUE(ip_secondary->sendFirmware(firmware));
  EXPECT_EQ(firmware, secondary._data);

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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::info);

  return RUN_ALL_TESTS();
}
