#include "aktualizr_secondary.h"

#include "aktualizr_secondary_discovery.h"

#include <future>

#include <sys/types.h>

#include "invstorage.h"
#include "logging.h"
#include "socket_activation.h"
#include "utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig &config)
    : config_(config), conn_(config.network.port) {}

void AktualizrSecondary::run() {
  // storage (share class with primary)
  boost::shared_ptr<INvStorage> storage = INvStorage::newStorage(config_.storage);

  // discovery
  AktualizrSecondaryDiscovery discovery(config_.network);
  std::thread discovery_thread = std::thread(&AktualizrSecondaryDiscovery::run, &discovery);

  // listen for messages
  std::shared_ptr<SecondaryPacket> pkt;
  while (conn_.in_channel_ >> pkt) {
    // TODO: process message
  }

  discovery.stop();
  discovery_thread.join();
}

void AktualizrSecondary::stop() { conn_.stop(); }

std::string AktualizrSecondary::getSerialResp() {
  LOG_ERROR << "getSerialResp is not implemented yet";
  return "";
}

std::string AktualizrSecondary::getHwIdResp() {
  LOG_ERROR << "getHwIdResp is not implemented yet";
  return "";
}

std::pair<KeyType, std::string> AktualizrSecondary::getPublicKeyResp() {
  LOG_ERROR << "getPublicKeyResp is not implemented yet";
  return std::make_pair(kUnknownKey, "");
}

Json::Value AktualizrSecondary::getManifestResp() {
  LOG_ERROR << "getManifestResp is not implemented yet";
  return Json::Value();
}

bool AktualizrSecondary::putMetadataResp(const Uptane::MetaPack &meta_pack) {
  (void)meta_pack;
  LOG_ERROR << "putMedatadatResp is not implemented yet";
  return false;
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) {
  (void)director;
  LOG_ERROR << "getRootVersionResp is not implemented yet";
  return -1;
}

bool AktualizrSecondary::putRootResp(Uptane::Root root, bool director) {
  (void)root;
  (void)director;
  LOG_ERROR << "putRootResp is not implemented yet";
  return false;
}

bool AktualizrSecondary::sendFirmwareResp(const std::string &firmware) {
  (void)firmware;
  LOG_ERROR << "sendFirmwareResp is not implemented yet";
  return false;
}
