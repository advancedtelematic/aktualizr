#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <array>
#include <memory>

#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

namespace Uptane {

SecondaryInterface::Ptr IpUptaneSecondary::connectAndCreate(const std::string& address, unsigned short port) {
  LOG_INFO << "Connecting to and getting info about IP Secondary: " << address << ":" << port << "...";

  ConnectionSocket con_sock{address, port};

  if (con_sock.connect() == 0) {
    LOG_INFO << "Connected to IP Secondary: "
             << "(" << address << ":" << port << ")";
  } else {
    LOG_WARNING << "Failed to connect to a Secondary: " << std::strerror(errno);
    return nullptr;
  }

  return create(address, port, *con_sock);
}

SecondaryInterface::Ptr IpUptaneSecondary::create(const std::string& address, unsigned short port, int con_fd) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_getInfoReq);

  auto m = req->getInfoReq();
  auto resp = Asn1Rpc(req, con_fd);

  if (resp->present() != AKIpUptaneMes_PR_getInfoResp) {
    LOG_ERROR << "IP Secondary failed to respond to information request at " << address << ":" << port;
    return std::make_shared<IpUptaneSecondary>(address, port, EcuSerial::Unknown(), HardwareIdentifier::Unknown(),
                                               PublicKey("", KeyType::kUnknown));
  }
  auto r = resp->getInfoResp();

  EcuSerial serial = EcuSerial(ToString(r->ecuSerial));
  HardwareIdentifier hw_id = HardwareIdentifier(ToString(r->hwId));
  std::string key = ToString(r->key);
  auto type = static_cast<KeyType>(r->keyType);
  PublicKey pub_key = PublicKey(key, type);

  LOG_INFO << "Got ECU information from IP Secondary: "
           << "hardware ID: " << hw_id << " serial: " << serial;

  return std::make_shared<IpUptaneSecondary>(address, port, serial, hw_id, pub_key);
}

SecondaryInterface::Ptr IpUptaneSecondary::connectAndCheck(const std::string& address, unsigned short port,
                                                           EcuSerial serial, HardwareIdentifier hw_id,
                                                           PublicKey pub_key) {
  // try to connect:
  // - if it succeeds compare with what we expect
  // - otherwise, keep using what we know
  try {
    auto sec = IpUptaneSecondary::connectAndCreate(address, port);
    if (sec != nullptr) {
      auto s = sec->getSerial();
      if (s != serial && serial != EcuSerial::Unknown()) {
        LOG_WARNING << "Expected IP Secondary at " << address << ":" << port << " with serial " << serial
                    << " but found " << s;
      }
      auto h = sec->getHwId();
      if (h != hw_id && hw_id != HardwareIdentifier::Unknown()) {
        LOG_WARNING << "Expected IP Secondary at " << address << ":" << port << " with hardware ID " << hw_id
                    << " but found " << h;
      }
      auto p = sec->getPublicKey();
      if (p.Type() == KeyType::kUnknown) {
        LOG_ERROR << "IP Secondary at " << address << ":" << port << " has an unknown key type!";
        return nullptr;
      } else if (p != pub_key && pub_key.Type() != KeyType::kUnknown) {
        LOG_WARNING << "Expected IP Secondary at " << address << ":" << port << " with public key:\n"
                    << pub_key.Value() << "... but found:\n"
                    << p.Value();
      }
      return sec;
    }
  } catch (std::exception& e) {
    LOG_WARNING << "Could not connect to IP Secondary at " << address << ":" << port << " with serial " << serial;
  }

  return std::make_shared<IpUptaneSecondary>(address, port, std::move(serial), std::move(hw_id), std::move(pub_key));
}

IpUptaneSecondary::IpUptaneSecondary(const std::string& address, unsigned short port, EcuSerial serial,
                                     HardwareIdentifier hw_id, PublicKey pub_key)
    : addr_{address, port}, serial_{std::move(serial)}, hw_id_{std::move(hw_id)}, pub_key_{std::move(pub_key)} {}

bool IpUptaneSecondary::putMetadata(const Target& target) {
  (void)target;
  Uptane::RawMetaPack meta_pack;
  if (!secondary_provider_->getDirectorMetadata(&meta_pack.director_root, &meta_pack.director_targets)) {
    LOG_ERROR << "Unable to read Director metadata.";
    return false;
  }
  if (!secondary_provider_->getImageRepoMetadata(&meta_pack.image_root, &meta_pack.image_timestamp,
                                                 &meta_pack.image_snapshot, &meta_pack.image_targets)) {
    LOG_ERROR << "Unable to read Image repo metadata.";
    return false;
  }

  LOG_INFO << "Sending Uptane metadata to the Secondary";

  bool put_result = putMetadata_v2(meta_pack);
  if (!put_result) {
    // Fall back to the previous metadata sending version.
    // TODO(OTA-4793): refactor to negotiate versioning once during
    // initialization.
    LOG_ERROR << "Failed to send metadata for target " << target.filename() << " to Secondary " << getSerial()
              << "\nFalling back to previous version of the metadata sending procedure.";

    put_result = putMetadata_v1(meta_pack);
  }
  return put_result;
}

bool IpUptaneSecondary::putMetadata_v1(const Uptane::RawMetaPack& meta_pack) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_putMetaReq);

  auto m = req->putMetaReq();
  m->image.present = image_PR_json;
  SetString(&m->image.choice.json.root, meta_pack.image_root);            // NOLINT
  SetString(&m->image.choice.json.targets, meta_pack.image_targets);      // NOLINT
  SetString(&m->image.choice.json.snapshot, meta_pack.image_snapshot);    // NOLINT
  SetString(&m->image.choice.json.timestamp, meta_pack.image_timestamp);  // NOLINT

  m->director.present = director_PR_json;
  SetString(&m->director.choice.json.root, meta_pack.director_root);        // NOLINT
  SetString(&m->director.choice.json.targets, meta_pack.director_targets);  // NOLINT

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_putMetaResp) {
    LOG_ERROR << "Failed to get response from sending metadata to Secondary";
    return false;
  }

  auto r = resp->putMetaResp();
  return r->result == AKInstallationResult_success;
}

bool IpUptaneSecondary::putMetadata_v2(const Uptane::RawMetaPack& meta_pack) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_putMetaReq2);

  auto m = req->putMetaReq2();
  m->directorRepo.present = directorRepo_PR_collection;
  m->imageRepo.present = imageRepo_PR_collection;

  auto director_root = Asn1Allocation<AKMetaJson_t>();
  SetString(&director_root->role, Uptane::Role::ROOT);
  SetString(&director_root->json, meta_pack.director_root);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->directorRepo.choice.collection, director_root);

  auto director_targets = Asn1Allocation<AKMetaJson_t>();
  SetString(&director_targets->role, Uptane::Role::TARGETS);
  SetString(&director_targets->json, meta_pack.director_targets);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->directorRepo.choice.collection, director_targets);

  auto image_root = Asn1Allocation<AKMetaJson_t>();
  SetString(&image_root->role, Uptane::Role::ROOT);
  SetString(&image_root->json, meta_pack.image_root);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->imageRepo.choice.collection, image_root);

  auto image_timestamp = Asn1Allocation<AKMetaJson_t>();
  SetString(&image_timestamp->role, Uptane::Role::TIMESTAMP);
  SetString(&image_timestamp->json, meta_pack.image_timestamp);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->imageRepo.choice.collection, image_timestamp);

  auto image_snapshot = Asn1Allocation<AKMetaJson_t>();
  SetString(&image_snapshot->role, Uptane::Role::SNAPSHOT);
  SetString(&image_snapshot->json, meta_pack.image_snapshot);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->imageRepo.choice.collection, image_snapshot);

  auto image_targets = Asn1Allocation<AKMetaJson_t>();
  SetString(&image_targets->role, Uptane::Role::TARGETS);
  SetString(&image_targets->json, meta_pack.image_targets);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  ASN_SEQUENCE_ADD(&m->imageRepo.choice.collection, image_targets);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_putMetaResp) {
    LOG_ERROR << "Failed to get response from sending metadata to Secondary";
    return false;
  }

  auto r = resp->putMetaResp();
  return r->result == AKInstallationResult_success;
}

Manifest IpUptaneSecondary::getManifest() const {
  LOG_DEBUG << "Getting the manifest from Secondary with serial " << getSerial();
  Asn1Message::Ptr req(Asn1Message::Empty());

  req->present(AKIpUptaneMes_PR_manifestReq);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_manifestResp) {
    LOG_ERROR << "Failed to get a response to a get manifest request to Secondary";
    return Json::Value();
  }
  auto r = resp->manifestResp();

  if (r->manifest.present != manifest_PR_json) {
    LOG_ERROR << "Manifest wasn't in json format";
    return Json::Value();
  }
  std::string manifest = ToString(r->manifest.choice.json);  // NOLINT
  return Utils::parseJSON(manifest);
}

bool IpUptaneSecondary::ping() const {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_getInfoReq);

  auto m = req->getInfoReq();

  auto resp = Asn1Rpc(req, getAddr());

  return resp->present() == AKIpUptaneMes_PR_getInfoResp;
}

data::ResultCode::Numeric IpUptaneSecondary::sendFirmware(const Uptane::Target& target) {
  auto send_result = sendFirmware_v2(target);
  if (send_result == data::ResultCode::Numeric::kUnknown) {
    // Fall back to the previous installation version.
    // TODO(OTA-4793): refactor to negotiate versioning once during
    // initialization.
    LOG_ERROR << "Failed to send firmware for target " << target.filename() << " to Secondary " << getSerial()
              << "\nFalling back to previous version of the installation procedure.";

    send_result = sendFirmware_v1(target);
  }
  return send_result;
}

data::ResultCode::Numeric IpUptaneSecondary::sendFirmware_v1(const Uptane::Target& target) {
  std::string data_to_send;

  if (target.IsOstree()) {
    // empty firmware means OSTree Secondaries: pack credentials instead
    data_to_send = secondary_provider_->getTreehubCredentials();
  } else {
    std::stringstream sstr;
    sstr << *secondary_provider_->getTargetFileHandle(target);
    data_to_send = sstr.str();
  }

  LOG_INFO << "Sending firmware to the Secondary, size: " << data_to_send.size();
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_sendFirmwareReq);

  auto m = req->sendFirmwareReq();
  SetString(&m->firmware, data_to_send);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_sendFirmwareResp) {
    LOG_ERROR << "Failed to get response to sending firmware to Secondary";
    return data::ResultCode::Numeric::kDownloadFailed;
    ;
  }

  auto r = resp->sendFirmwareResp();
  if (r->result == AKInstallationResult_success) {
    return data::ResultCode::Numeric::kOk;
    ;
  }
  return data::ResultCode::Numeric::kDownloadFailed;
  ;
}

data::ResultCode::Numeric IpUptaneSecondary::sendFirmware_v2(const Uptane::Target& target) {
  LOG_INFO << "Instructing Secondary " << getSerial() << " to receive target " << target.filename();
  if (target.IsOstree()) {
    return downloadOstreeRev(target);
  } else {
    return uploadFirmware(target);
  }
}

data::ResultCode::Numeric IpUptaneSecondary::install(const Uptane::Target& target) {
  std::lock_guard<std::mutex> l(install_mutex);
  auto install_result = install_v2(target);
  if (install_result == data::ResultCode::Numeric::kUnknown) {
    // Fall back to the previous installation version.
    // TODO(OTA-4793): refactor to negotiate versioning once during
    // initialization.
    LOG_ERROR << "Failed to install " << target.filename() << " on Secondary " << getSerial()
              << "\nFalling back to the previous version of the installation procedure.";

    install_result = install_v1(target);
  }
  return install_result;
}

data::ResultCode::Numeric IpUptaneSecondary::install_v1(const Uptane::Target& target) {
  LOG_INFO << "Invoking an installation of the target on the Secondary: " << target.filename();

  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_installReq);

  // prepare request message
  auto req_mes = req->installReq();
  SetString(&req_mes->hash, target.filename());
  // send request and receive response, a request-response type of RPC
  auto resp = Asn1Rpc(req, getAddr());

  // invalid type of an response message
  if (resp->present() != AKIpUptaneMes_PR_installResp) {
    LOG_ERROR << "Failed to get response to an installation request to Secondary";
    return data::ResultCode::Numeric::kInternalError;
  }

  // deserialize the response message
  auto r = resp->installResp();

  return static_cast<data::ResultCode::Numeric>(r->result);
}

data::ResultCode::Numeric IpUptaneSecondary::install_v2(const Uptane::Target& target) {
  LOG_INFO << "Instructing Secondary " << getSerial() << " to install target " << target.filename();
  return invokeInstallOnSecondary(target);
}

data::ResultCode::Numeric IpUptaneSecondary::downloadOstreeRev(const Uptane::Target& target) {
  LOG_INFO << "Instructing Secondary ( " << getSerial() << " ) to download OSTree commit ( " << target.sha256Hash()
           << " )";
  const std::string tls_creds = secondary_provider_->getTreehubCredentials();
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(static_cast<AKIpUptaneMes_PR>(AKIpUptaneMes_PR_downloadOstreeRevReq));

  auto m = req->downloadOstreeRevReq();
  SetString(&m->tlsCred, tls_creds);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_downloadOstreeRevResp) {
    LOG_ERROR << "Failed to get response to download ostree revision request";
    return data::ResultCode::Numeric::kUnknown;
  }

  auto r = resp->downloadOstreeRevResp();
  return static_cast<data::ResultCode::Numeric>(r->result);
}

data::ResultCode::Numeric IpUptaneSecondary::uploadFirmware(const Uptane::Target& target) {
  LOG_INFO << "Uploading the target image (" << target.filename() << ") "
           << "to the Secondary (" << getSerial() << ")";

  data::ResultCode::Numeric upload_result = data::ResultCode::Numeric::kDownloadFailed;

  std::unique_ptr<StorageTargetRHandle> image_reader = secondary_provider_->getTargetFileHandle(target);

  auto image_size = image_reader->rsize();
  const size_t size = 1024;
  size_t total_send_data = 0;
  std::array<uint8_t, size> buf{};
  data::ResultCode::Numeric upload_data_result = data::ResultCode::Numeric::kOk;

  while (total_send_data < image_size && upload_data_result == data::ResultCode::Numeric::kOk) {
    auto read_data = image_reader->rread(buf.data(), buf.size());
    upload_data_result = uploadFirmwareData(buf.data(), read_data);
    total_send_data += read_data;
  }
  if (upload_data_result == data::ResultCode::Numeric::kOk && total_send_data == image_size) {
    upload_result = data::ResultCode::Numeric::kOk;
  } else {
    upload_result = (upload_data_result != data::ResultCode::Numeric::kOk) ? upload_data_result
                                                                           : data::ResultCode::Numeric::kDownloadFailed;
  }
  image_reader->rclose();
  return upload_result;
}

data::ResultCode::Numeric IpUptaneSecondary::uploadFirmwareData(const uint8_t* data, size_t size) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_uploadDataReq);

  auto m = req->uploadDataReq();
  OCTET_STRING_fromBuf(&m->data, reinterpret_cast<const char*>(data), static_cast<int>(size));
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() == AKIpUptaneMes_PR_NOTHING) {
    LOG_ERROR << "Failed to get response to an uploading data request to Secondary";
    return data::ResultCode::Numeric::kUnknown;
  }
  if (resp->present() != AKIpUptaneMes_PR_uploadDataResp) {
    LOG_ERROR << "Invalid response to an uploading data request to Secondary";
    return data::ResultCode::Numeric::kInternalError;
  }

  auto r = resp->uploadDataResp();
  return (r->result == AKInstallationResult_success) ? data::ResultCode::Numeric::kOk
                                                     : data::ResultCode::Numeric::kDownloadFailed;
}

data::ResultCode::Numeric IpUptaneSecondary::invokeInstallOnSecondary(const Uptane::Target& target) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_installReq);

  // prepare request message
  auto req_mes = req->installReq();
  SetString(&req_mes->hash, target.filename());
  // send request and receive response, a request-response type of RPC
  auto resp = Asn1Rpc(req, getAddr());

  // invalid type of an response message
  if (resp->present() != AKIpUptaneMes_PR_installResp) {
    LOG_ERROR << "Failed to get response to an installation request to Secondary";
    return data::ResultCode::Numeric::kUnknown;
  }

  // deserialize the response message
  auto r = resp->installResp();

  return static_cast<data::ResultCode::Numeric>(r->result);
}

}  // namespace Uptane
