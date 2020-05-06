#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

#include <memory>

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
    LOG_ERROR << "Failed to get info response message from Secondary";
    throw std::runtime_error("Failed to obtain information about a Secondary: " + address + std::to_string(port));
  }
  auto r = resp->getInfoResp();

  EcuSerial serial = EcuSerial(ToString(r->ecuSerial));
  HardwareIdentifier hw_id = HardwareIdentifier(ToString(r->hwId));
  std::string key = ToString(r->key);
  auto type = static_cast<KeyType>(r->keyType);
  PublicKey pub_key = PublicKey(key, type);

  LOG_INFO << "Got info from IP Secondary: "
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
      if (s != serial) {
        LOG_ERROR << "Mismatch between Secondary serials " << s << " and " << serial;
        return nullptr;
      }
      auto h = sec->getHwId();
      if (h != hw_id) {
        LOG_ERROR << "Mismatch between hardware IDs " << h << " and " << hw_id;
        return nullptr;
      }
      auto p = sec->getPublicKey();
      if (pub_key.Type() == KeyType::kUnknown) {
        LOG_INFO << "Secondary " << s << " do not have a known public key";
      } else if (p != pub_key) {
        LOG_ERROR << "Mismatch between public keys " << p.Value() << " and " << pub_key.Value() << " for Secondary "
                  << serial;
        return nullptr;
      }
      return sec;
    }
  } catch (std::exception& e) {
    LOG_WARNING << "Could not connect to Secondary " << serial << " at " << address << ":" << port
                << " using previously known registration data";
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
    LOG_ERROR << "Failed to get response to sending manifest to Secondary";
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
    // fallback to the previous installation version
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
    // fallback to the previous installation version
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
  uint8_t buf[size];
  data::ResultCode::Numeric upload_data_result = data::ResultCode::Numeric::kOk;

  while (total_send_data < image_size && upload_data_result == data::ResultCode::Numeric::kOk) {
    auto read_data = image_reader->rread(buf, sizeof(buf));
    upload_data_result = uploadFirmwareData(buf, read_data);
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
