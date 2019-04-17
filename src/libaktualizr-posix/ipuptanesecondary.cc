#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "ipuptanesecondary.h"
#include "logging/logging.h"

#include <memory>

namespace Uptane {

std::shared_ptr<Uptane::SecondaryInterface> IpUptaneSecondary::create(const std::string& address, unsigned short port) {
  LOG_INFO << "Creating IP Secondary: " << address << ":" << port << "...";

  EcuSerial serial = EcuSerial::Unknown();
  HardwareIdentifier hw_id = HardwareIdentifier::Unknown();
  PublicKey pub_key;

  struct sockaddr_in sock_address {};

  memset(&sock_address, 0, sizeof(sock_address));
  sock_address.sin_family = AF_INET;
  inet_pton(AF_INET, address.c_str(), &(sock_address.sin_addr));
  sock_address.sin_port = htons(port);

  // get secondary serial and HW ID
  {
    LOG_INFO << "Getting the secondary's hardware ID and serial number...";
    Asn1Message::Ptr req(Asn1Message::Empty());
    req->present(AKIpUptaneMes_PR_getInfoReq);

    auto resp = Asn1Rpc(req, sock_address);

    if (resp->present() != AKIpUptaneMes_PR_getInfoResp) {
      LOG_ERROR << "Failed to get info response message from secondary";
      // TODO: make it inline with the overall exception strategy
      throw std::runtime_error("Failed to obtain information about a secondary: " + address + std::to_string(port));
    }
    auto r = resp->getInfoResp();

    serial = EcuSerial(ToString(r->ecuSerial));
    hw_id = HardwareIdentifier(ToString(r->hwId));
  }

  // get pub key
  {
    LOG_INFO << "Getting the secondary's public key...";
    Asn1Message::Ptr req(Asn1Message::Empty());

    req->present(AKIpUptaneMes_PR_publicKeyReq);

    auto resp = Asn1Rpc(req, sock_address);

    if (resp->present() != AKIpUptaneMes_PR_publicKeyResp) {
      LOG_ERROR << "Failed to get public key response message from secondary";
      throw std::runtime_error("Failed to obtain information about a secondary: " + address + std::to_string(port));
    }
    auto r = resp->publicKeyResp();

    std::string key = ToString(r->key);

    auto type = static_cast<KeyType>(r->type);
    pub_key = PublicKey(key, type);
  }
  return std::make_shared<IpUptaneSecondary>(sock_address, serial, hw_id, pub_key);
}

IpUptaneSecondary::IpUptaneSecondary(const sockaddr_in& sock_addr, EcuSerial serial, HardwareIdentifier hw_id,
                                     PublicKey pub_key)
    : sock_address_{sock_addr}, serial_{std::move(serial)}, hw_id_{std::move(hw_id)}, pub_key_{std::move(pub_key)} {}

bool IpUptaneSecondary::putMetadata(const RawMetaPack& meta_pack) {
  LOG_INFO << "Sending Uptane metadata to the secondary";
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
    LOG_ERROR << "Failed to get response to sending manifest to secondary";
    return false;
  }

  auto r = resp->putMetaResp();
  return r->result == AKInstallationResult_success;
}

bool IpUptaneSecondary::sendFirmware(const std::shared_ptr<std::string>& data) {
  std::lock_guard<std::mutex> l(install_mutex);
  LOG_INFO << "Sending firmware to the secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_sendFirmwareReq);

  auto m = req->sendFirmwareReq();
  SetString(&m->firmware, *data);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_sendFirmwareResp) {
    LOG_ERROR << "Failed to get response to sending firmware to secondary";
    return false;
  }

  auto r = resp->sendFirmwareResp();
  return r->result == AKInstallationResult_success;
}

Json::Value IpUptaneSecondary::getManifest() {
  LOG_INFO << "Getting the manifest key of a secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());

  req->present(AKIpUptaneMes_PR_manifestReq);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_manifestResp) {
    LOG_ERROR << "Failed to get public key response message from secondary";
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
}  // namespace Uptane
