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

enum class HandlerVersion { kV1, kV2, kV2Failure };

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
                const Uptane::Manifest& manifest, HandlerVersion handler_version = HandlerVersion::kV2)
      : serial_(serial),
        hdw_id_(hdw_id),
        pub_key_(pub_key),
        manifest_(manifest),
        image_filepath_{image_dir_ / "image.bin"},
        hasher_{MultiPartHasher::create(Hash::Type::kSha256)},
        handler_version_(handler_version) {
    registerBaseHandlers();
    if (handler_version_ == HandlerVersion::kV1) {
      registerV1Handlers();
    } else if (handler_version_ == HandlerVersion::kV2) {
      registerV2Handlers();
    } else {
      registerV2FailureHandlers();
    }
  }

 public:
  const std::string verification_failure = "Expected verification test failure";
  const std::string upload_data_failure = "Expected data upload test failure";
  const std::string ostree_failure = "Expected OSTree download test failure";
  const std::string installation_failure = "Expected installation test failure";

  const Uptane::EcuSerial& serial() const { return serial_; }
  const Uptane::HardwareIdentifier& hwID() const { return hdw_id_; }
  const PublicKey& publicKey() const { return pub_key_; }
  const Uptane::Manifest& manifest() const { return manifest_; }
  const Uptane::MetaBundle& metadata() const { return meta_bundle_; }
  HandlerVersion handler_version() const { return handler_version_; }

  Hash getReceivedImageHash() const { return hasher_->getHash(); }

  size_t getReceivedImageSize() const { return boost::filesystem::file_size(image_filepath_); }

  const std::string& getReceivedTlsCreds() const { return tls_creds_; }

  // Used by both protocol versions:
  void registerBaseHandlers() {
    registerHandler(AKIpUptaneMes_PR_getInfoReq,
                    std::bind(&SecondaryMock::getInfoHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_manifestReq,
                    std::bind(&SecondaryMock::getManifestHdlr, this, std::placeholders::_1, std::placeholders::_2));
  }

  // Used by protocol v1 (deprecated, no longer implemented in production code) only:
  void registerV1Handlers() {
    registerHandler(AKIpUptaneMes_PR_putMetaReq,
                    std::bind(&SecondaryMock::putMetaHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_sendFirmwareReq,
                    std::bind(&SecondaryMock::sendFirmwareHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_installReq,
                    std::bind(&SecondaryMock::installHdlr, this, std::placeholders::_1, std::placeholders::_2));
  }

  // Used by protocol v2 (based on current aktualizr-secondary implementation) only:
  void registerV2Handlers() {
    registerHandler(AKIpUptaneMes_PR_putMetaReq2,
                    std::bind(&SecondaryMock::putMeta2Hdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_uploadDataReq,
                    std::bind(&SecondaryMock::uploadDataHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_downloadOstreeRevReq,
                    std::bind(&SecondaryMock::downloadOstreeRev, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_installReq,
                    std::bind(&SecondaryMock::install2Hdlr, this, std::placeholders::_1, std::placeholders::_2));
  }

  // Procotol v2 handlers that fail in predictable ways.
  void registerV2FailureHandlers() {
    registerHandler(AKIpUptaneMes_PR_putMetaReq2,
                    std::bind(&SecondaryMock::putMeta2FailureHdlr, this, std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_uploadDataReq, std::bind(&SecondaryMock::uploadDataFailureHdlr, this,
                                                              std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_downloadOstreeRevReq, std::bind(&SecondaryMock::downloadOstreeRevFailure, this,
                                                                     std::placeholders::_1, std::placeholders::_2));

    registerHandler(AKIpUptaneMes_PR_installReq,
                    std::bind(&SecondaryMock::install2FailureHdlr, this, std::placeholders::_1, std::placeholders::_2));
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

    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Root()),
                         ToString(md->director.choice.json.root));
    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Director(), Uptane::Role::Targets()),
                         ToString(md->director.choice.json.targets));

    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Root()),
                         ToString(md->image.choice.json.root));
    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()),
                         ToString(md->image.choice.json.timestamp));
    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()),
                         ToString(md->image.choice.json.snapshot));
    meta_bundle_.emplace(std::make_pair(Uptane::RepositoryType::Image(), Uptane::Role::Targets()),
                         ToString(md->image.choice.json.targets));

    out_msg.present(AKIpUptaneMes_PR_putMetaResp).putMetaResp()->result = AKInstallationResult_success;

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

    data::InstallationResult result = putMetadata2(meta_bundle);

    auto m = out_msg.present(AKIpUptaneMes_PR_putMetaResp2).putMetaResp2();
    m->result = static_cast<AKInstallationResultCode_t>(result.result_code.num_code);
    SetString(&m->description, result.description);

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode putMeta2FailureHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    auto m = out_msg.present(AKIpUptaneMes_PR_putMetaResp2).putMetaResp2();
    m->result = static_cast<AKInstallationResultCode_t>(data::ResultCode::Numeric::kVerificationFailed);
    SetString(&m->description, verification_failure);

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode installHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    auto result = install(ToString(in_msg.installReq()->hash)).result_code.num_code;

    out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result =
        static_cast<AKInstallationResultCode_t>(result);

    if (data::ResultCode::Numeric::kNeedCompletion == result) {
      return ReturnCode::kRebootRequired;
    }

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode install2Hdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    auto result = install(ToString(in_msg.installReq()->hash));

    auto m = out_msg.present(AKIpUptaneMes_PR_installResp2).installResp2();
    m->result = static_cast<AKInstallationResultCode_t>(result.result_code.num_code);
    SetString(&m->description, result.description);

    if (data::ResultCode::Numeric::kNeedCompletion == result.result_code.num_code) {
      return ReturnCode::kRebootRequired;
    }

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode install2FailureHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    auto m = out_msg.present(AKIpUptaneMes_PR_installResp2).installResp2();
    m->result = static_cast<AKInstallationResultCode_t>(data::ResultCode::Numeric::kInstallFailed);
    SetString(&m->description, installation_failure);

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode uploadDataHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    if (in_msg.uploadDataReq()->data.size < 0) {
      out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp()->result = AKInstallationResult_failure;
      return ReturnCode::kOk;
    }

    size_t data_size = static_cast<size_t>(in_msg.uploadDataReq()->data.size);
    auto result = receiveImageData(in_msg.uploadDataReq()->data.buf, data_size);

    auto m = out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp();
    m->result = static_cast<AKInstallationResultCode_t>(result.result_code.num_code);
    SetString(&m->description, result.description);

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode uploadDataFailureHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    auto m = out_msg.present(AKIpUptaneMes_PR_uploadDataResp).uploadDataResp();
    m->result = static_cast<AKInstallationResultCode_t>(data::ResultCode::Numeric::kDownloadFailed);
    SetString(&m->description, upload_data_failure);

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode sendFirmwareHdlr(Asn1Message& in_msg, Asn1Message& out_msg) {
    received_firmware_data_ = ToString(in_msg.sendFirmwareReq()->firmware);
    out_msg.present(AKIpUptaneMes_PR_sendFirmwareResp).sendFirmwareResp()->result = AKInstallationResult_success;

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode downloadOstreeRev(Asn1Message& in_msg, Asn1Message& out_msg) {
    tls_creds_ = ToString(in_msg.downloadOstreeRevReq()->tlsCred);
    auto m = out_msg.present(AKIpUptaneMes_PR_downloadOstreeRevResp).downloadOstreeRevResp();
    m->result = static_cast<AKInstallationResultCode_t>(data::ResultCode::Numeric::kOk);
    SetString(&m->description, "");

    return ReturnCode::kOk;
  }

  MsgHandler::ReturnCode downloadOstreeRevFailure(Asn1Message& in_msg, Asn1Message& out_msg) {
    (void)in_msg;

    auto m = out_msg.present(AKIpUptaneMes_PR_downloadOstreeRevResp).downloadOstreeRevResp();
    m->result = static_cast<AKInstallationResultCode_t>(data::ResultCode::Numeric::kDownloadFailed);
    SetString(&m->description, ostree_failure);

    return ReturnCode::kOk;
  }

  data::InstallationResult putMetadata2(const Uptane::MetaBundle& meta_bundle) {
    meta_bundle_ = meta_bundle;
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  }

  data::InstallationResult receiveImageData(const uint8_t* data, size_t size) {
    std::ofstream target_file(image_filepath_.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);

    target_file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    hasher_->update(data, size);

    target_file.close();
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  }

  data::InstallationResult install(const std::string& target_name) {
    if (!received_firmware_data_.empty()) {
      // data was received via the old request (sendFirmware)
      if (target_name == "OSTREE") {
        tls_creds_ = received_firmware_data_;
      } else {
        receiveImageData(reinterpret_cast<const uint8_t*>(received_firmware_data_.c_str()),
                         received_firmware_data_.size());
      }
    }
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
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
  HandlerVersion handler_version_;
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

  Uptane::Target createTarget(std::shared_ptr<PackageManagerInterface>& package_manager_) {
    Json::Value target_json;
    target_json["custom"]["targetFormat"] = "BINARY";
    target_json["hashes"]["sha256"] = hash().HashString();
    target_json["length"] = size();
    Uptane::Target target = Uptane::Target(path(), target_json);

    auto fhandle = package_manager_->createTargetFile(target);
    const std::string content = Utils::readFile(path());
    fhandle.write(const_cast<char*>(content.c_str()), static_cast<std::streamsize>(size()));
    fhandle.close();

    return target;
  }

 private:
  TemporaryDirectory target_dir_;
  const size_t image_size_;
  boost::filesystem::path image_filepath_;
  Hash image_hash_;
};

class SecondaryRpcTest : public ::testing::Test,
                         public ::testing::WithParamInterface<std::pair<size_t, HandlerVersion>> {
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

  void sendAndInstallBinaryImage() {
    Uptane::Target target = image_file_.createTarget(package_manager_);
    const HandlerVersion handler_version = secondary_.handler_version();

    data::InstallationResult result = ip_secondary_->putMetadata(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kVerificationFailed);
      EXPECT_EQ(result.description, secondary_.verification_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
      verifyMetadata(secondary_.metadata());
    }

    result = ip_secondary_->sendFirmware(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kDownloadFailed);
      EXPECT_EQ(result.description, secondary_.upload_data_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
    }

    result = ip_secondary_->install(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kInstallFailed);
      EXPECT_EQ(result.description, secondary_.installation_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
      EXPECT_EQ(image_file_.hash(), secondary_.getReceivedImageHash());
    }
  }

  void installOstreeRev() {
    Json::Value target_json;
    target_json["custom"]["targetFormat"] = "OSTREE";
    Uptane::Target target = Uptane::Target("OSTREE", target_json);

    const HandlerVersion handler_version = secondary_.handler_version();

    data::InstallationResult result = ip_secondary_->putMetadata(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kVerificationFailed);
      EXPECT_EQ(result.description, secondary_.verification_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
      verifyMetadata(secondary_.metadata());
    }

    result = ip_secondary_->sendFirmware(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kDownloadFailed);
      EXPECT_EQ(result.description, secondary_.ostree_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
    }

    result = ip_secondary_->install(target);
    if (handler_version == HandlerVersion::kV2Failure) {
      EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kInstallFailed);
      EXPECT_EQ(result.description, secondary_.installation_failure);
    } else {
      EXPECT_TRUE(result.isSuccess());
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

  sendAndInstallBinaryImage();

  installOstreeRev();
}

/* These tests use a mock of most of the Secondary internals in order to test
 * the RPC mechanism between the Primary and IP Secondary. The tests cover the
 * old/v1/fallback handlers as well as the new/v2 versions. */
INSTANTIATE_TEST_SUITE_P(
    SecondaryRpcTestCases, SecondaryRpcTest,
    ::testing::Values(std::make_pair(1, HandlerVersion::kV2), std::make_pair(1024, HandlerVersion::kV2),
                      std::make_pair(1024 - 1, HandlerVersion::kV2), std::make_pair(1024 + 1, HandlerVersion::kV2),
                      std::make_pair(1024 * 10 + 1, HandlerVersion::kV2), std::make_pair(1, HandlerVersion::kV1),
                      std::make_pair(1024, HandlerVersion::kV1), std::make_pair(1024 - 1, HandlerVersion::kV1),
                      std::make_pair(1024 + 1, HandlerVersion::kV1), std::make_pair(1024 * 10 + 1, HandlerVersion::kV1),
                      std::make_pair(1024, HandlerVersion::kV2Failure)));

TEST(SecondaryTcpServer, TestIpSecondaryIfSecondaryIsNotRunning) {
  in_port_t secondary_port = TestUtils::getFreePortAsInt();
  SecondaryInterface::Ptr ip_secondary;

  // Try to connect to a non-running Secondary and create a corresponding instance on Primary.
  ip_secondary = Uptane::IpUptaneSecondary::connectAndCreate("localhost", secondary_port);
  EXPECT_EQ(ip_secondary, nullptr);

  // Create Secondary on Primary without actually connecting to Secondary.
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

  // Expect nothing since the Secondary is not running.
  EXPECT_EQ(ip_secondary->getManifest(), Json::Value());

  TargetFile target_file("mytarget_image.img");
  Uptane::Target target = target_file.createTarget(package_manager);

  // Expect failures since the Secondary is not running.
  EXPECT_FALSE(ip_secondary->putMetadata(target).isSuccess());
  EXPECT_FALSE(ip_secondary->sendFirmware(target).isSuccess());
  EXPECT_FALSE(ip_secondary->install(target).isSuccess());
}

/* This class returns a positive result for every message. The test cases verify
 * that the implementation can recover from situations where something goes wrong. */
class SecondaryRpcTestPositive : public ::testing::Test, public MsgHandler {
 protected:
  SecondaryRpcTestPositive()
      : secondary_server_{*this, "", 0}, secondary_server_thread_{[&]() { secondary_server_.run(); }} {
    secondary_server_.wait_until_running();
  }

  ~SecondaryRpcTestPositive() {
    secondary_server_.stop();
    secondary_server_thread_.join();
  }

  // Override default implementation with a stub that always returns success.
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

/* This test fails because the Secondary TCP server implementation is a
 * single-threaded, synchronous and blocking hence it cannot accept any new
 * connections until the current one is closed. Therefore, if a client/Primary
 * does not close its socket for some reason then Secondary becomes
 * "unavailable" */
// TEST_F(SecondaryRpcTestPositive, primaryNotClosingSocket) {
//  ConnectionSocket con_sock{"127.0.0.1", secondary_server_.port()};
//  con_sock.connect();
//  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
//}

TEST_F(SecondaryRpcTestPositive, primaryConnectAndDisconnect) {
  ConnectionSocket{"127.0.0.1", secondary_server_.port()}.connect();
  // do a valid request/response exchange to verify if Secondary works as expected
  // after accepting and closing a new connection
  ASSERT_EQ(sendInstallMsg(), AKIpUptaneMes_PR_installResp);
}

TEST_F(SecondaryRpcTestPositive, primaryConnectAndSendValidButNotSupportedMsg) {
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

TEST_F(SecondaryRpcTestPositive, primaryConnectAndSendBrokenAsn1Msg) {
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

TEST_F(SecondaryRpcTestPositive, primaryConnectAndSendGarbage) {
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
