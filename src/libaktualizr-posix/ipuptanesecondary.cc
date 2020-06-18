#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <array>
#include <memory>

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

/* Determine the best protocol version to use for this Secondary. This did not
 * exist for v1 and thus only works for v2 and beyond. It would be great if we
 * could just do this once, but we do not have a simple way to do that,
 * especially because of Secondaries that need to reboot to complete
 * installation. */
void IpUptaneSecondary::getSecondaryVersion() const {
  LOG_DEBUG << "Negotiating the protocol version with Secondary " << getSerial();
  const uint32_t latest_version = 2;
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_versionReq);
  auto m = req->versionReq();
  m->version = latest_version;
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_versionResp) {
    // Bad response probably means v1, but make sure the Secondary is actually
    // responsive before assuming that.
    if (ping()) {
      LOG_DEBUG << "Failed to get a response to a version request from Secondary " << getSerial()
                << "; assuming version 1.";
      protocol_version = 1;
    } else {
      LOG_INFO << "Failed to get a response to a version request from Secondary " << getSerial()
               << "; unable to determine protocol version.";
    }
    return;
  }

  auto r = resp->versionResp();
  const auto secondary_version = static_cast<uint32_t>(r->version);
  if (secondary_version <= latest_version) {
    LOG_DEBUG << "Using protocol version " << secondary_version << " for Secondary " << getSerial();
    protocol_version = secondary_version;
  } else {
    LOG_ERROR << "Secondary protocol version is " << secondary_version << " but Primary only supports up to "
              << latest_version << "! Communication will most likely fail!";
    protocol_version = latest_version;
  }
}

data::InstallationResult IpUptaneSecondary::putMetadata(const Target& target) {
  Uptane::MetaBundle meta_bundle;
  if (!secondary_provider_->getMetadata(&meta_bundle, target)) {
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                    "Unable to load stored metadata from Primary");
  }

  getSecondaryVersion();

  LOG_INFO << "Sending Uptane metadata to the Secondary";
  data::InstallationResult put_result;
  if (protocol_version == 2) {
    put_result = putMetadata_v2(meta_bundle);
  } else if (protocol_version == 1) {
    put_result = putMetadata_v1(meta_bundle);
  } else {
    LOG_ERROR << "Unexpected protocol version: " << protocol_version;
    put_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                          "Unexpected protocol version: " + std::to_string(protocol_version));
  }
  return put_result;
}

data::InstallationResult IpUptaneSecondary::putMetadata_v1(const Uptane::MetaBundle& meta_bundle) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_putMetaReq);

  auto m = req->putMetaReq();
  m->director.present = director_PR_json;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->director.choice.json.root,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Root()));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->director.choice.json.targets,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));

  m->image.present = image_PR_json;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->image.choice.json.root,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Root()));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->image.choice.json.timestamp,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->image.choice.json.snapshot,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  SetString(&m->image.choice.json.targets,
            getMetaFromBundle(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_putMetaResp) {
    LOG_ERROR << "Failed to get response from sending metadata to Secondary";
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                    "Failed to get response from sending metadata to Secondary");
  }

  auto r = resp->putMetaResp();
  if (r->result == AKInstallationResult_success) {
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  } else {
    return data::InstallationResult(data::ResultCode::Numeric::kVerificationFailed,
                                    "Error sending metadata to Secondary");
  }
}

void IpUptaneSecondary::addMetadata(const Uptane::MetaBundle& meta_bundle, const Uptane::RepositoryType repo,
                                    const Uptane::Role& role, AKMetaCollection_t& collection) {
  auto meta_json = Asn1Allocation<AKMetaJson_t>();
  SetString(&meta_json->role, role.ToString());
  SetString(&meta_json->json, getMetaFromBundle(meta_bundle, repo, role));
  ASN_SEQUENCE_ADD(&collection, meta_json);
}

data::InstallationResult IpUptaneSecondary::putMetadata_v2(const Uptane::MetaBundle& meta_bundle) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_putMetaReq2);

  auto m = req->putMetaReq2();
  m->directorRepo.present = directorRepo_PR_collection;
  m->imageRepo.present = imageRepo_PR_collection;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  addMetadata(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Root(), m->directorRepo.choice.collection);
  addMetadata(meta_bundle, Uptane::RepositoryType::Director(), Uptane::Role::Targets(),
              m->directorRepo.choice.collection);  // NOLINT(cppcoreguidelines-pro-type-union-access)
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  addMetadata(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Root(), m->imageRepo.choice.collection);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  addMetadata(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp(), m->imageRepo.choice.collection);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  addMetadata(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot(), m->imageRepo.choice.collection);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
  addMetadata(meta_bundle, Uptane::RepositoryType::Image(), Uptane::Role::Targets(), m->imageRepo.choice.collection);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_putMetaResp2) {
    LOG_ERROR << "Failed to get response from sending metadata to Secondary";
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                    "Failed to get response from sending metadata to Secondary");
  }

  auto r = resp->putMetaResp2();
  return data::InstallationResult(static_cast<data::ResultCode::Numeric>(r->result), ToString(r->description));
}

Manifest IpUptaneSecondary::getManifest() const {
  getSecondaryVersion();

  LOG_DEBUG << "Getting the manifest from Secondary with serial " << getSerial();
  Asn1Message::Ptr req(Asn1Message::Empty());

  req->present(AKIpUptaneMes_PR_manifestReq);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_manifestResp) {
    LOG_ERROR << "Failed to get a response to a manifest request to Secondary with serial " << getSerial();
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

data::InstallationResult IpUptaneSecondary::sendFirmware(const Uptane::Target& target) {
  data::InstallationResult send_result;
  if (protocol_version == 2) {
    send_result = sendFirmware_v2(target);
  } else if (protocol_version == 1) {
    send_result = sendFirmware_v1(target);
  } else {
    LOG_ERROR << "Unexpected protocol version: " << protocol_version;
    send_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                           "Unexpected protocol version: " + std::to_string(protocol_version));
  }
  return send_result;
}

data::InstallationResult IpUptaneSecondary::sendFirmware_v1(const Uptane::Target& target) {
  std::string data_to_send;

  if (target.IsOstree()) {
    // empty firmware means OSTree Secondaries: pack credentials instead
    data_to_send = secondary_provider_->getTreehubCredentials();
  } else {
    std::stringstream sstr;
    auto str = secondary_provider_->getTargetFileHandle(target);
    sstr << str.rdbuf();
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
    return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed,
                                    "Failed to get response to sending firmware to Secondary");
  }

  auto r = resp->sendFirmwareResp();
  if (r->result == AKInstallationResult_success) {
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  }
  return data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "");
}

data::InstallationResult IpUptaneSecondary::sendFirmware_v2(const Uptane::Target& target) {
  LOG_INFO << "Instructing Secondary " << getSerial() << " to receive target " << target.filename();
  if (target.IsOstree()) {
    return downloadOstreeRev(target);
  } else {
    return uploadFirmware(target);
  }
}

data::InstallationResult IpUptaneSecondary::install(const Uptane::Target& target) {
  data::InstallationResult install_result;
  if (protocol_version == 2) {
    install_result = install_v2(target);
  } else if (protocol_version == 1) {
    install_result = install_v1(target);
  } else {
    LOG_ERROR << "Unexpected protocol version: " << protocol_version;
    install_result = data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                              "Unexpected protocol version: " + std::to_string(protocol_version));
  }

  return install_result;
}

data::InstallationResult IpUptaneSecondary::install_v1(const Uptane::Target& target) {
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
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                    "Failed to get response to an installation request to Secondary");
  }

  // deserialize the response message
  auto r = resp->installResp();

  return data::InstallationResult(static_cast<data::ResultCode::Numeric>(r->result), "");
}

data::InstallationResult IpUptaneSecondary::install_v2(const Uptane::Target& target) {
  LOG_INFO << "Instructing Secondary " << getSerial() << " to install target " << target.filename();
  return invokeInstallOnSecondary(target);
}

data::InstallationResult IpUptaneSecondary::downloadOstreeRev(const Uptane::Target& target) {
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
    return data::InstallationResult(data::ResultCode::Numeric::kUnknown,
                                    "Failed to get response to download ostree revision request");
  }

  auto r = resp->downloadOstreeRevResp();
  return data::InstallationResult(static_cast<data::ResultCode::Numeric>(r->result), ToString(r->description));
}

data::InstallationResult IpUptaneSecondary::uploadFirmware(const Uptane::Target& target) {
  LOG_INFO << "Uploading the target image (" << target.filename() << ") "
           << "to the Secondary (" << getSerial() << ")";

  auto upload_result = data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "");

  auto image_reader = secondary_provider_->getTargetFileHandle(target);

  uint64_t image_size = target.length();
  const size_t size = 1024;
  size_t total_send_data = 0;
  std::array<uint8_t, size> buf{};
  auto upload_data_result = data::InstallationResult(data::ResultCode::Numeric::kOk, "");

  while (total_send_data < image_size && upload_data_result.isSuccess()) {
    image_reader.read(reinterpret_cast<char*>(buf.data()), buf.size());
    upload_data_result = uploadFirmwareData(buf.data(), static_cast<size_t>(image_reader.gcount()));
    total_send_data += static_cast<size_t>(image_reader.gcount());
  }
  if (upload_data_result.isSuccess() && total_send_data == image_size) {
    upload_result = data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  } else if (!upload_data_result.isSuccess()) {
    upload_result = upload_data_result;
  } else {
    upload_result = data::InstallationResult(data::ResultCode::Numeric::kDownloadFailed, "Incomplete upload");
  }
  image_reader.close();
  return upload_result;
}

data::InstallationResult IpUptaneSecondary::uploadFirmwareData(const uint8_t* data, size_t size) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_uploadDataReq);

  auto m = req->uploadDataReq();
  OCTET_STRING_fromBuf(&m->data, reinterpret_cast<const char*>(data), static_cast<int>(size));
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() == AKIpUptaneMes_PR_NOTHING) {
    LOG_ERROR << "Failed to get response to an uploading data request to Secondary";
    return data::InstallationResult(data::ResultCode::Numeric::kUnknown,
                                    "Failed to get response to an uploading data request to Secondary");
  }
  if (resp->present() != AKIpUptaneMes_PR_uploadDataResp) {
    LOG_ERROR << "Invalid response to an uploading data request to Secondary";
    return data::InstallationResult(data::ResultCode::Numeric::kInternalError,
                                    "Invalid response to an uploading data request to Secondary");
  }

  auto r = resp->uploadDataResp();
  return data::InstallationResult(static_cast<data::ResultCode::Numeric>(r->result), ToString(r->description));
}

data::InstallationResult IpUptaneSecondary::invokeInstallOnSecondary(const Uptane::Target& target) {
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_installReq);

  // prepare request message
  auto req_mes = req->installReq();
  SetString(&req_mes->hash, target.filename());
  // send request and receive response, a request-response type of RPC
  auto resp = Asn1Rpc(req, getAddr());

  // invalid type of an response message
  if (resp->present() != AKIpUptaneMes_PR_installResp2) {
    LOG_ERROR << "Failed to get response to an installation request to Secondary";
    return data::InstallationResult(data::ResultCode::Numeric::kUnknown,
                                    "Failed to get response to an installation request to Secondary");
  }

  // deserialize the response message
  auto r = resp->installResp2();
  return data::InstallationResult(static_cast<data::ResultCode::Numeric>(r->result), ToString(r->description));
}

}  // namespace Uptane
