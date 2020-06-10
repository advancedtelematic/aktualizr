#include <gtest/gtest.h>

#include <thread>

#include <netinet/tcp.h>

#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "msg_handler.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "secondary_tcp_server.h"
#include "storage/invstorage.h"
#include "test_utils.h"

/* This class allows us to divert messages from the regular handlers in
 * AktualizrSecondary to our own test functions. This lets us test only what was
 * received by the Secondary but not how it was processed.
 *
 * It also has handlers for both the old/v1 and new/v2 versions of the RPC
 * protocol, so this is how we prove that the Primary is still
 * backwards-compatible with older/v1 Secondaries. */
class SecondaryMock : public MsgDispatcher {
 public:
  SecondaryMock(const Uptane::EcuSerial& serial, const Uptane::HardwareIdentifier& hdw_id, const PublicKey& pub_key,
                const Uptane::Manifest& manifest, bool new_install_msgs = true)
      : serial_(serial),
        hdw_id_(hdw_id),
        pub_key_(pub_key),
        manifest_(manifest),
        image_filepath_{image_dir_ / "image.bin"},
        hasher_{MultiPartHasher::create(Hash::Type::kSha256)} {
    registerBaseHandlers();
    if (new_install_msgs) {
      registerHandlersForNewRequests();
    } else {
      registerHandlersForOldRequests();
    }
  }

 public:
  const Uptane::EcuSerial& serial() const { return serial_; }
  const Uptane::HardwareIdentifier& hwID() const { return hdw_id_; }
  const PublicKey& publicKey() const { return pub_key_; }
  const Uptane::Manifest& manifest() const { return manifest_; }
  const Uptane::MetaBundle& metadata() const { return meta_bundle_; }

  Hash getReceivedImageHash() const { return hasher_->getHash(); }

  size_t getReceivedImageSize() const { return boost::filesystem::file_size(image_filepath_); }

  const std::string& getReceivedTlsCreds() const { return tls_creds_; }

  void registerBaseHandlers() {
    registerHandler(AKIpUptaneMes_PR_getInfoReq,
                    std::bind(&SecondaryMock::getInfoHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_manifestReq,
                    std::bind(&SecondaryMock::getManifestHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_installReq,
                    std::bind(&SecondaryMock::installHdlr, this, std::placeholders::_1, std::placeholders::_2));
  }

  void registerHandlersForNewRequests() {
    registerHandler(AKIpUptaneMes_PR_putMetaReq2,
                    std::bind(&SecondaryMock::putMeta2Hdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_uploadDataReq,
                    std::bind(&SecondaryMock::uploadDataHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_downloadOstreeRevReq,
                    std::bind(&SecondaryMock::downloadOstreeRev, this, std::placeholders::_1, std::placeholders::_2));
  }

  void registerHandlersForOldRequests() {
    registerHandler(AKIpUptaneMes_PR_putMetaReq,
                    std::bind(&SecondaryMock::putMetaHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_sendFirmwareReq,
                    std::bind(&SecondaryMock::sendFirmwareHdlr, this, std::placeholders::_1, std::placeholders::_2));
  }

 private:
  MsgHandler::ReturnCode getInfoHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    out_msg.present(AKIpUptaneMes_PR_getInfoResp);

    auto info_resp = out_msg.getInfoResp();

    SetString(&info_resp->ecuSerial, serial_.ToString());
    SetString(&info_resp->hwId, hdw_id_.ToString());
    info_resp->keyType = static_cast<AKIpUptaneKeyType_t>(pub_key_.Type());
    SetString(&info_resp->key, pub_key_.Value());

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode getManifestHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    out_msg.present(AKIpUptaneMes_PR_manifestResp);
    auto manifest_resp = out_msg.manifestResp();
    manifest_resp->manifest.present = manifest_PR_json;
    SetString(&manifest_resp->manifest.choice.json, Utils::jsonToStr(manifest()));

    return ReturnCode::kOk;
  }

  // This is basically the old implemention from AktualizrSecondary.
  MsgHandler::ReturnCode putMetaHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    auto md = in_msg.putMetaReq();
    Uptane::MetaBundle meta_bundle;

    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()),
                        ToString(md->director.choice.json.root));
    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
                        ToString(md->director.choice.json.targets));

    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()),
                        ToString(md->image.choice.json.root));
    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
                        ToString(md->image.choice.json.timestamp));
    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()),
                        ToString(md->image.choice.json.snapshot));
    meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets()),
                        ToString(md->image.choice.json.targets));

    bool ok = putMetadata(meta_bundle);

    out_msg.present(AKIpUptaneMes_PR_putMetaResp).putMetaResp()->result =
        ok ? AKInstallationResult_success : AKInstallationResult_failure;

    return ReturnCode::kOk;
  }

  // This is annoyingly similar to AktualizrSecondary::putMetaHdlr().
  MsgHandler::ReturnCode putMeta2Hdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    auto md = in_msg.putMetaReq2();
    Uptane::MetaBundle meta_bundle;

    EXPECT_EQ(md->directorRepo.present, directorRepo_PR_collection);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const int director_meta_count = md->directorRepo.choice.collection.list.count;
    EXPECT_EQ(director_meta_count, 2);
    for (int i = 0; i < director_meta_count; i++) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-union-access)
      const AKMetaJson_t object = *md->directorRepo.choice.collection.list.array[i];
      const std::string role = ToString(object.role);
      std::string json = ToString(object.json);
      if (role == Uptane::Role::ROOT) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()), std::move(json));
      } else if (role == Uptane::Role::TARGETS) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
                            std::move(json));
      }
    }

    EXPECT_EQ(md->imageRepo.present, imageRepo_PR_collection);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    const int image_meta_count = md->imageRepo.choice.collection.list.count;
    EXPECT_EQ(image_meta_count, 4);
    for (int i = 0; i < image_meta_count; i++) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-union-access)
      const AKMetaJson_t object = *md->imageRepo.choice.collection.list.array[i];
      const std::string role = ToString(object.role);
      std::string json = ToString(object.json);
      if (role == Uptane::Role::ROOT) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()), std::move(json));
      } else if (role == Uptane::Role::TIMESTAMP) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
                            std::move(json));
      } else if (role == Uptane::Role::SNAPSHOT) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()), std::move(json));
      } else if (role == Uptane::Role::TARGETS) {
        meta_bundle.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets()), std::move(json));
      }
    }

    bool ok = putMetadata(meta_bundle);

    out_msg.present(AKIpUptaneMes_PR_putMetaResp).putMetaResp()->result =
        ok ? AKInstallationResult_success : AKInstallationResult_failure;

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode installHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    auto install_result = install(ToString(in_msg.installReq()->hash));
    out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result =
        static_cast<AKInstallationResultCode_t>(install_result);

    if (data::ResultCode::Numeric::kNeedCompletion == install_result) {
      return ReturnCode::kRebootRequired;
    }

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode uploadDataHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    if (in_msg.uploadDataReq()->data.size < 0) {
      out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp()->result = AKInstallationResult_failure;
      return ReturnCode::kOk;
    }

    size_t data_size = static_cast<size_t>(in_msg.uploadDataReq()->data.size);
    auto send_firmware_result = receiveImageData(in_msg.uploadDataReq()->data.buf, data_size);

    out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp()->result =
        (send_firmware_result == data::ResultCode::Numeric::kOk) ? AKInstallationResult_success
                                                                 : AKInstallationResult_failure;

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode sendFirmwareHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    received_firmware_data_ = ToString(in_msg.sendFirmwareReq()->firmware);
    out_msg.present(AKIpUptaneMes_PR_sendFirmwareResp).sendFirmwareResp()->result = AKInstallationResult_success;

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode downloadOstreeRev(Asn1Message& in_msg, Asn1Message& out_msg) {
    tls_creds_ = ToString(in_msg.downloadOstreeRevReq()->tlsCred);
    out_msg.present(AKIpUptaneMes_PR_downloadOstreeRevResp).downloadOstreeRevResp()->result =
        AKInstallationResultCode_ok;

    return ReturnCode::kOk;
  }

  bool putMetadata(const Uptane::MetaBundle& meta_bundle) {
    meta_bundle_ = meta_bundle;
    return true;
  }

  data::ResultCode::Numeric receiveImageData(const uint8_t* data, size_t size) {
    std::ofstream target_file(image_filepath_.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);

    target_file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    hasher_->update(data, size);

    target_file.close();
    return data::ResultCode::Numeric::kOk;
  }

  data::ResultCode::Numeric install(const std::string& target_name) {
    if (!received_firmware_data_.empty()) {
      // data was received via the old request (sendFirmware)
      if (target_name == "OSTREE") {
        tls_creds_ = received_firmware_data_;
      } else {
        receiveImageData(reinterpret_cast<const uint8_t*>(received_firmware_data_.c_str()),
                         received_firmware_data_.size());
      }
    }
    return data::ResultCode::Numeric::kOk;
  }

 private:
  const Uptane::EcuSerial serial_;
  const Uptane::HardwareIdentifier hdw_id_;
  const PublicKey pub_key_;
  const Uptane::Manifest manifest_;

  Uptane::MetaBundle meta_bundle_;

  TemporaryDirectory image_dir_;
  boost::filesystem::path image_filepath_;
  std::shared_ptr<MultiPartHasher> hasher_;
  std::unordered_map<unsigned int, Handler> handler_map_;
  std::string tls_creds_;
  std::string received_firmware_data_;
};

class TargetFile {
 public:
  TargetFile(const std::string filename, size_t size = 1024, Hash::Type hash_type = Hash::Type::kSha256)
      : image_size_{size},
        image_filepath_{target_dir_ / filename},
        image_hash_{generateRandomFile(image_filepath_, size, hash_type)} {}

  std::string path() const { return image_filepath_.string(); }
  const Hash& hash() const { return image_hash_; }
  const size_t& size() const { return image_size_; }

  static Hash generateRandomFile(const boost::filesystem::path& filepath, size_t size, Hash::Type hash_type) {
    auto hasher = MultiPartHasher::create(hash_type);
    std::ofstream file{filepath.string(), std::ofstream::binary};

    if (!file.is_open() || !file.good()) {
      throw std::runtime_error("Failed to create a file: " + filepath.string());
    }

    const unsigned char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv";
    unsigned char cur_symbol;

    for (unsigned int ii = 0; ii < size; ++ii) {
      cur_symbol = symbols[static_cast<unsigned int>(rand()) % sizeof(symbols)];
      file.put(static_cast<char>(cur_symbol));
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

class SecondaryRpcTest : public ::testing::Test, public ::testing::WithParamInterface<std::pair<size_t, bool>> {
 public:
  const std::string ca_ = "ca";
  const std::string cert_ = "cert";
  const std::string pkey_ = "pkey";
  const std::string server_ = "ostree-server";
  const std::string director_root_ = "director-root";
  const std::string director_targets_ = "director-targets";
  const std::string image_root_ = "image-root";
  const std::string image_timestamp_ = "image-timestamp";
  const std::string image_snapshot_ = "image-snapshot";
  const std::string image_targets_ = "image-targets";

 protected:
  SecondaryRpcTest()
      : secondary_{Uptane::EcuSerial("serial"), Uptane::HardwareIdentifier("hardware-id"),
                   PublicKey("pub-key", KeyType::kED25519), Uptane::Manifest(), GetParam().second},
        secondary_server_{secondary_, "", 0},
        secondary_server_thread_{std::bind(&SecondaryRpcTest::run_secondary_server, this)},
        image_file_{"mytarget_image.img", GetParam().first} {
    secondary_server_.wait_until_running();
    ip_secondary_ = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_server_.port());

    config_.pacman.ostree_server = server_;
    config_.pacman.type = PACKAGE_MANAGER_NONE;
    config_.pacman.images_path = temp_dir_.Path() / "images";
    config_.storage.path = temp_dir_.Path();

    storage_ = INvStorage::newStorage(config_.storage);
    storage_->storeTlsCreds(ca_, cert_, pkey_);
    storage_->storeRoot(director_root_, Uptane::RepositoryType::Director(), Uptane::Version(1));
    storage_->storeNonRoot(director_targets_, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
    storage_->storeRoot(image_root_, Uptane::RepositoryType::Image(), Uptane::Version(1));
    storage_->storeNonRoot(image_timestamp_, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
    storage_->storeNonRoot(image_snapshot_, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());
    storage_->storeNonRoot(image_targets_, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

    package_manager_ = PackageManagerFactory::makePackageManager(config_.pacman, config_.bootloader, storage_, nullptr);
    secondary_provider_ = std::make_shared<SecondaryProvider>(config_, storage_, package_manager_);
    ip_secondary_->init(secondary_provider_);
  }

  ~SecondaryRpcTest() {
    secondary_server_.stop();
    secondary_server_thread_.join();
  }

  void run_secondary_server() { secondary_server_.run(); }

  void verifyMetadata(const Uptane::MetaBundle& meta_bundle) {
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Root()),
              director_root_);
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
              director_targets_);
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Root()),
              image_root_);
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
              image_timestamp_);
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()),
              image_snapshot_);
    EXPECT_EQ(Uptane::getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Targets()),
              image_targets_);
  }

  data::ResultCode::Numeric sendAndInstallBinaryImage() {
    Json::Value target_json;
    target_json["custom"]["targetFormat"] = "BINARY";
    target_json["hashes"]["sha256"] = image_file_.hash().HashString();
    target_json["length"] = image_file_.size();
    Uptane::Target target = Uptane::Target(image_file_.path(), target_json);

    auto fhandle = package_manager_->createTargetFile(target);
    const std::string content = Utils::readFile(image_file_.path());
    fhandle.write(const_cast<char*>(content.c_str()), static_cast<std::streamsize>(image_file_.size()));
    fhandle.close();

    EXPECT_TRUE(ip_secondary_->putMetadata(target));
    verifyMetadata(secondary_.metadata());

    data::ResultCode::Numeric result = ip_secondary_->sendFirmware(target);
    if (result != data::ResultCode::Numeric::kOk) {
      return result;
    }
    return ip_secondary_->install(target);
  }

  data::ResultCode::Numeric installOstreeRev() {
    Json::Value target_json;
    target_json["custom"]["targetFormat"] = "OSTREE";
    Uptane::Target target = Uptane::Target("OSTREE", target_json);

    EXPECT_TRUE(ip_secondary_->putMetadata(target));
    verifyMetadata(secondary_.metadata());

    data::ResultCode::Numeric result = ip_secondary_->sendFirmware(target);
    if (result != data::ResultCode::Numeric::kOk) {
      return result;
    }
    return ip_secondary_->install(target);
  }

  SecondaryMock secondary_;
  std::shared_ptr<SecondaryProvider> secondary_provider_;
  SecondaryTcpServer secondary_server_;
  std::thread secondary_server_thread_;
  TargetFile image_file_;
  SecondaryInterface::Ptr ip_secondary_;
  TemporaryDirectory temp_dir_;
  std::shared_ptr<INvStorage> storage_;
  Config config_;
  std::shared_ptr<PackageManagerInterface> package_manager_;
};

// Test the serialization/deserialization and the TCP/IP communication implementation
// that occurs during communication between Primary and IP Secondary
TEST_P(SecondaryRpcTest, AllRpcCallsTest) {
  ASSERT_TRUE(ip_secondary_ != nullptr) << "Failed to create IP Secondary";
  EXPECT_EQ(ip_secondary_->getSerial(), secondary_.serial());
  EXPECT_EQ(ip_secondary_->getHwId(), secondary_.hwID());
  EXPECT_EQ(ip_secondary_->getPublicKey(), secondary_.publicKey());
  EXPECT_EQ(ip_secondary_->getManifest(), secondary_.manifest());

  EXPECT_EQ(sendAndInstallBinaryImage(), data::ResultCode::Numeric::kOk);
  EXPECT_EQ(image_file_.hash(), secondary_.getReceivedImageHash());

  EXPECT_EQ(installOstreeRev(), data::ResultCode::Numeric::kOk);

  std::string archive = secondary_.getReceivedTlsCreds();
  {
    std::stringstream as(archive);
    EXPECT_EQ(ca_, Utils::readFileFromArchive(as, "ca.pem"));
  }
  {
    std::stringstream as(archive);
    EXPECT_EQ(cert_, Utils::readFileFromArchive(as, "client.pem"));
  }
  {
    std::stringstream as(archive);
    EXPECT_EQ(pkey_, Utils::readFileFromArchive(as, "pkey.pem"));
  }
  {
    std::stringstream as(archive);
    EXPECT_EQ(server_, Utils::readFileFromArchive(as, "server.url", true));
  }
}

INSTANTIATE_TEST_SUITE_P(SecondaryRpcTestCases, SecondaryRpcTest,
                         ::testing::Values(
                             // Binary update by using a new RPC API bteween Primary and Secondary
                             std::make_pair(1024 * 3 + 1, true /* new API/messages */),

                             // Binary update that tests the Primary's fallback to the previous/old version of RPC API
                             // The Secondary mock is configured to handle the old RPC API (sendFirmware) while
                             // Primary uses the new API and if it fails fallbacks to the old API usage
                             // (false|true) defines whether to handle the new or old RPC API in the Secondary mock
                             // `true` means to handle the new RPC API
                             // `false` means to handle an old RPC API (sendFirmware request) * /
                             std::make_pair(1024 * 3 + 1, false /* old messages/sendFirmware, fallback testing */),
                             std::make_pair(1, true), std::make_pair(1024, true), std::make_pair(1024 - 1, true),
                             std::make_pair(1024 + 1, true), std::make_pair(1024 * 10, false),
                             std::make_pair(1024 * 10 - 1, false), std::make_pair(1024 * 10 + 1, false)));

TEST(SecondaryTcpServer, TestIpSecondaryIfSecondaryIsNotRunning) {
  in_port_t secondary_port = TestUtils::getFreePortAsInt();
  SecondaryInterface::Ptr ip_secondary;

  // trying to connect to a non-running Secondary and create a corresponding instance on Primary
  ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_port);
  EXPECT_EQ(ip_secondary, nullptr);

  // create Primary's secondary without connecting to Secondary
  ip_secondary = std::make_shared<Uptane::IpUptaneSecondary>("localhost", secondary_port, Uptane::EcuSerial("serial"),
                                                             Uptane::HardwareIdentifier("hwid"),
                                                             PublicKey("key", KeyType::kED25519));

  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.pacman.images_path = temp_dir.Path() / "images";
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<PackageManagerInterface> package_manager =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  std::shared_ptr<SecondaryProvider> secondary_provider =
      std::make_shared<SecondaryProvider>(config, storage, package_manager);
  ip_secondary->init(secondary_provider);

  TargetFile target_file("mytarget_image.img");

  // expect failures since the secondary is not running
  EXPECT_EQ(ip_secondary->getManifest(), Json::Value());

  Json::Value target_json;
  target_json["custom"]["targetFormat"] = "BINARY";
  target_json["hashes"]["sha256"] = target_file.hash().HashString();
  target_json["length"] = target_file.size();
  Uptane::Target target = Uptane::Target(target_file.path(), target_json);

  auto fhandle = package_manager->createTargetFile(target);
  const std::string content = Utils::readFile(target_file.path());
  fhandle.write(const_cast<char*>(content.c_str()), static_cast<std::streamsize>(target_file.size()));
  fhandle.close();

  EXPECT_FALSE(ip_secondary->putMetadata(target));
  EXPECT_NE(ip_secondary->sendFirmware(target), data::ResultCode::Numeric::kOk);
  EXPECT_NE(ip_secondary->install(target), data::ResultCode::Numeric::kOk);
}

class SecondaryRpcTestNegative : public ::testing::Test, public MsgHandler {
 protected:
  SecondaryRpcTestNegative()
      : secondary_server_{*this, "", 0}, secondary_server_thread_{[&]() { secondary_server_.run(); }} {
    secondary_server_.wait_until_running();
  }

  ~SecondaryRpcTestNegative() {
    secondary_server_.stop();
    secondary_server_thread_.join();
  }

  ReturnCode handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) override {
    (void)in_msg;
    out_msg->present(AKIpUptaneMes_PR_installResp).installResp()->result = AKInstallationResultCode_ok;
    return ReturnCode::kOk;
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
