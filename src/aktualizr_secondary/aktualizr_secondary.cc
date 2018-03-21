#include "aktualizr_secondary.h"

#include <future>

#include <sys/types.h>

#include "invstorage.h"
#include "logging.h"
#include "socket_activation.h"
#include "utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig &config, const boost::shared_ptr<INvStorage> &storage)
    : config_(config), conn_(config.network.port), storage_(storage), keys_(storage_, config.keymanagerConfig()) {
  pacman = PackageManagerFactory::makePackageManager(config_.pacman, storage_);
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }
}

// Initialize the parts needed for a secondary
// - ECU keys
// - serial id
bool AktualizrSecondary::uptaneInitialize() {
  if (keys_.generateUptaneKeyPair().size() == 0) {
    return false;
  }

  // from uptane/initialize.cc but we only take care of our own serial/hwid
  std::vector<std::pair<std::string, std::string> > ecu_serials;

  if (storage_->loadEcuSerials(&ecu_serials)) {
    ecu_serial_ = ecu_serials[0].first;
    hardware_id_ = ecu_serials[0].second;
    return true;
  }

  std::string ecu_serial_local = config_.uptane.ecu_serial;
  if (ecu_serial_local.empty()) {
    ecu_serial_local = Crypto::getKeyId(keys_.getUptanePublicKey());
  }

  std::string ecu_hardware_id = config_.uptane.ecu_hardware_id;
  if (ecu_hardware_id.empty()) {
    ecu_hardware_id = Utils::getHostname();
    if (ecu_hardware_id == "") return false;
  }

  ecu_serials.push_back(std::pair<std::string, std::string>(ecu_serial_local, ecu_hardware_id));
  storage_->storeEcuSerials(ecu_serials);
  ecu_serial_ = ecu_serials[0].first;
  hardware_id_ = ecu_serials[0].second;

  return true;
}

void AktualizrSecondary::run() {
  // listen for messages
  std::shared_ptr<SecondaryPacket> pkt;
  while (conn_.in_channel_ >> pkt) {
    // TODO: process message
  }
}

void AktualizrSecondary::stop() { conn_.stop(); }

std::string AktualizrSecondary::getSerialResp() const { return ecu_serial_; }

std::string AktualizrSecondary::getHwIdResp() const { return hardware_id_; }

std::pair<KeyType, std::string> AktualizrSecondary::getPublicKeyResp() const {
  return std::make_pair(config_.uptane.key_type, keys_.getUptanePublicKey());
}

Json::Value AktualizrSecondary::getManifestResp() const{
  return pacman->getManifest(getSerialResp());
}

bool AktualizrSecondary::putMetadataResp(const Uptane::MetaPack &meta_pack) {
  (void)meta_pack;
  LOG_ERROR << "putMedatadatResp is not implemented yet";
  return false;
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) const {
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
