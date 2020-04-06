#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

#include <memory>

namespace Uptane {

Uptane::SecondaryInterface::Ptr IpUptaneSecondary::connectAndCreate(const std::string& address, unsigned short port,
                                                                    ImageReader image_reader,
                                                                    TlsCredsProvider treehub_cred_provider) {
  LOG_INFO << "Connecting to and getting info about IP Secondary: " << address << ":" << port << "...";

  ConnectionSocket con_sock{address, port};

  if (con_sock.connect() == 0) {
    LOG_INFO << "Connected to IP Secondary: "
             << "(" << address << ":" << port << ")";
  } else {
    LOG_WARNING << "Failed to connect to a Secondary: " << std::strerror(errno);
    return nullptr;
  }

  return create(address, port, *con_sock, image_reader, treehub_cred_provider);
}

Uptane::SecondaryInterface::Ptr IpUptaneSecondary::create(const std::string& address, unsigned short port, int con_fd,
                                                          ImageReader image_reader,
                                                          TlsCredsProvider treehub_cred_provider) {
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

  return std::make_shared<IpUptaneSecondary>(address, port, serial, hw_id, pub_key, image_reader,
                                             treehub_cred_provider);
}

SecondaryInterface::Ptr IpUptaneSecondary::connectAndCheck(const std::string& address, unsigned short port,
                                                           EcuSerial serial, HardwareIdentifier hw_id,
                                                           PublicKey pub_key, ImageReader image_reader,
                                                           TlsCredsProvider treehub_cred_provider) {
  // try to connect:
  // - if it succeeds compare with what we expect
  // - otherwise, keep using what we know
  try {
    auto sec = IpUptaneSecondary::connectAndCreate(address, port, image_reader, treehub_cred_provider);
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

  return std::make_shared<IpUptaneSecondary>(address, port, std::move(serial), std::move(hw_id), std::move(pub_key),
                                             image_reader, treehub_cred_provider);
}

IpUptaneSecondary::IpUptaneSecondary(const std::string& address, unsigned short port, EcuSerial serial,
                                     HardwareIdentifier hw_id, PublicKey pub_key, ImageReader image_reader,
                                     TlsCredsProvider treehub_cred_provider)
    : addr_{address, port},
      serial_{std::move(serial)},
      hw_id_{std::move(hw_id)},
      pub_key_{std::move(pub_key)},
      image_reader_{image_reader},
      treehub_cred_provider_{treehub_cred_provider} {}

bool IpUptaneSecondary::putMetadata(const RawMetaPack& meta_pack) {
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

data::ResultCode::Numeric IpUptaneSecondary::install(const Uptane::Target& target) {
  return install_v2(target);
  // return install_v1(target);
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

data::ResultCode::Numeric IpUptaneSecondary::install_v1(const Uptane::Target& target) {
  std::string data_to_send;

  if (target.IsOstree()) {
    // empty firmware means OSTree Secondaries: pack credentials instead
    data_to_send = treehub_cred_provider_();
  } else {
    std::stringstream sstr;
    sstr << *image_reader_(target);
    data_to_send = sstr.str();
  }

  bool send_frimware_result = sendFirmware(data_to_send);
  if (!send_frimware_result) {
    return data::ResultCode::Numeric::kInstallFailed;
  }

  LOG_INFO << "Invoking an installation of the target on the Secondary: " << target;

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
  if (target.IsOstree()) {
    // empty firmware means OSTree secondaries: pack credentials instead
    std::string data_to_send = treehub_cred_provider_();
    bool send_frimware_result = sendFirmware(data_to_send);
    if (!send_frimware_result) {
      return data::ResultCode::Numeric::kInstallFailed;
    }
  } else {
    std::unique_ptr<StorageTargetRHandle> image_reader = image_reader_(target);

    auto image_size = image_reader->rsize();
    const size_t size = 1024;
    size_t total_read_data = 0;
    uint8_t buf[size];
    bool send_frimware_result = false;

    while (total_read_data < image_size) {
      auto read_data = image_reader->rread(buf, sizeof(buf));
      total_read_data += read_data;
      send_frimware_result = sendFirmwareData(buf, read_data);
      if (!send_frimware_result) {
        return data::ResultCode::Numeric::kInstallFailed;
      }
    }

    image_reader->rclose();
  }

  LOG_INFO << "Invoking an installation of the target on the secondary: " << target;

  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_installReq);

  // prepare request message
  auto req_mes = req->installReq();
  SetString(&req_mes->hash, target.filename());
  // send request and receive response, a request-response type of RPC
  auto resp = Asn1Rpc(req, getAddr());

  // invalid type of an response message
  if (resp->present() != AKIpUptaneMes_PR_installResp) {
    LOG_ERROR << "Failed to get response to an installation request to secondary";
    return data::ResultCode::Numeric::kInternalError;
  }

  // deserialize the response message
  auto r = resp->installResp();

  return static_cast<data::ResultCode::Numeric>(r->result);
}

bool IpUptaneSecondary::sendFirmware(const std::string& data) {
  std::lock_guard<std::mutex> l(install_mutex);
  LOG_INFO << "Sending firmware to the Secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_sendFirmwareReq);

  auto m = req->sendFirmwareReq();
  SetString(&m->firmware, data);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_sendFirmwareResp) {
    LOG_ERROR << "Failed to get response to sending firmware to secondary";
    return false;
  }

  auto r = resp->sendFirmwareResp();
  return r->result == AKInstallationResult_success;
}

bool IpUptaneSecondary::sendFirmwareData(const uint8_t* data, size_t size) {
  std::lock_guard<std::mutex> l(install_mutex);
  LOG_INFO << "Sending firmware to the secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_sendFirmwareDataReq);

  auto m = req->sendFirmwareDataReq();
  OCTET_STRING_fromBuf(&m->data, reinterpret_cast<const char*>(data), static_cast<int>(size));
  m->size = size;
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_sendFirmwareResp) {
    LOG_ERROR << "Failed to get response to sending firmware to secondary";
    return false;
  }

  auto r = resp->sendFirmwareResp();
  return r->result == AKInstallationResult_success;
}

}  // namespace Uptane
