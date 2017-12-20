#include <string>

#include <openssl/x509.h>
#include <boost/scoped_array.hpp>

#include "bootstrap.h"
#include "logger.h"
#include "uptane/uptanerepository.h"

namespace Uptane {

// Postcondition: device_id is in the storage
bool Repository::initDeviceId(const ProvisionConfig& provision_config, const UptaneConfig& uptane_config,
                              const TlsConfig& tls_config) {
  // if device_id is already stored, just return
  std::string device_id;
  if (storage.loadDeviceId(&device_id)) return true;

  // if device_id is specified in config, just use it, otherwise generate a  random one
  device_id = uptane_config.device_id;
  if (device_id.empty()) {
    if (provision_config.mode == kAutomatic) {
      device_id = Utils::genPrettyName();
    } else if (provision_config.mode == kImplicit) {
      std::string cert;
      if (tls_config.cert_source == kFile) {
        if (!storage.loadTlsCert(&cert)) {
          LOGGER_LOG(LVL_error, "Certificate is not found, can't extract device_id");
          return false;
        }
      } else {  // kPkcs11
#ifdef BUILD_P11
        if (!p11.readTlsCert(&cert)) {
          LOGGER_LOG(LVL_error, "Certificate is not found, can't extract device_id");
          return false;
        }
#else
        LOGGER_LOG(LVL_error, "Aktualizr was built without PKCS#11 support, can't extract device_id");
        return false;
#endif
      }
      if (!Crypto::extractSubjectCN(cert, &device_id)) {
        LOGGER_LOG(LVL_error, "Couldn't extract CN from the certificate");
        return false;
      }
    } else {
      LOGGER_LOG(LVL_error, "Unknown provisioning method");
      return false;
    }
  }

  storage.storeDeviceId(device_id);
  return true;
}
void Repository::resetDeviceId() { storage.clearDeviceId(); }

void Repository::setEcuSerialMembers(const std::pair<std::string, std::string>& ecu_serials) {
  primary_ecu_serial = ecu_serials.first;
  primary_hardware_id_ = ecu_serials.second;
}

// Postcondition [(serial, hw_id)] is in the storage
bool Repository::initEcuSerials(const UptaneConfig& uptane_config) {
  std::vector<std::pair<std::string, std::string> > ecu_serials;

  // TODO: the assumption now is that the set of connected ECUs doesn't change, but it might obviously
  //   not be the case. ECU discovery seems to be a big story and should be worked on accordingly.
  if (storage.loadEcuSerials(&ecu_serials)) {
    setEcuSerialMembers(ecu_serials[0]);
    return true;
  }

  std::string primary_ecu_serial_local = uptane_config.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = primary_public_key_id;
  }

  std::string primary_ecu_hardware_id = uptane_config.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
  }

  ecu_serials.push_back(std::pair<std::string, std::string>(primary_ecu_serial_local, primary_ecu_hardware_id));

  std::map<std::string, std::pair<std::string, std::string> >::const_iterator it;
  for (it = secondary_info.begin(); it != secondary_info.end(); ++it) {
    ecu_serials.push_back(std::pair<std::string, std::string>(it->first, it->second.first));
  }

  storage.storeEcuSerials(ecu_serials);
  setEcuSerialMembers(ecu_serials[0]);
  return true;
}

void Repository::resetEcuSerials() {
  storage.clearEcuSerials();
  primary_ecu_serial = "";
}

void Repository::setEcuKeysMembers(const std::string& primary_public, const std::string& primary_private,
                                   const std::string& primary_public_id, CryptoSource source) {
  primary_public_key_uri = primary_public;
  if (source == kPkcs11)
    primary_private_key_uri = primary_public_key_uri;
  else
    primary_private_key_uri = primary_private;
  primary_public_key_id = primary_public_id;
  key_source = source;
}

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
bool Repository::initPrimaryEcuKeys(const UptaneConfig& uptane_config) {
  std::string primary_public;
  std::string primary_private;
  std::string primary_public_id;

#ifdef BUILD_P11
  if (uptane_config.key_source == kPkcs11) {
    primary_public = p11.getUptaneKeyId();
    std::string public_key_content;

    // dummy read to check if the key is present
    if (!p11.readUptanePublicKey(&public_key_content))
      if (!p11.generateUptaneKeyPair()) return false;

    // realy read the key
    if (!p11.readUptanePublicKey(&public_key_content)) return false;

    primary_public_id = Crypto::getKeyId(public_key_content);
    setEcuKeysMembers(primary_public, primary_private, primary_public_id, kPkcs11);
    return true;
  }
#endif
  if (uptane_config.key_source == kFile) {
    if (storage.loadPrimaryKeys(&primary_public, &primary_private)) {
      primary_public_id = Crypto::getKeyId(primary_public);
      setEcuKeysMembers(primary_public, primary_private, primary_public_id, kFile);
      return true;
    }
    if (!Crypto::generateRSAKeyPair(&primary_public, &primary_private)) return false;
  }

  primary_public_id = Crypto::getKeyId(primary_public);
  storage.storePrimaryKeys(primary_public, primary_private);
  setEcuKeysMembers(primary_public, primary_private, primary_public_id, kFile);
  return true;
}

void Repository::resetEcuKeys() {
  storage.clearPrimaryKeys();
  primary_public_key_uri = "";
  primary_private_key_uri = "";
  primary_public_key_id = "";
}

bool Repository::loadSetTlsCreds(const TlsConfig& tls_config) {
  std::string pkey;
  std::string cert;
  std::string ca;

  bool res = true;

#ifdef BUILD_P11
  if (tls_config.pkey_source == kFile) {
    res = res && storage.loadTlsPkey(&pkey);
    pkcs11_tls_keyname = "";
  } else {  // kPkcs11
    pkey = p11.getTlsPkeyId();
    pkcs11_tls_keyname = pkey;
  }

  if (tls_config.cert_source == kFile) {
    res = res && storage.loadTlsCert(&cert);
    pkcs11_tls_certname = "";
  } else {  // kPkcs11
    cert = p11.getTlsCertId();
    pkcs11_tls_certname = cert;
  }

  if (tls_config.ca_source == kFile) {
    res = res && storage.loadTlsCa(&ca);
  } else {  // kPkcs11
    ca = p11.getTlsCacertId();
  }
#else
  res = storage.loadTlsCreds(&ca, &cert, &pkey);
#endif

  if (res) http.setCerts(ca, tls_config.ca_source, cert, tls_config.cert_source, pkey, tls_config.pkey_source);

  return res;
}

// Postcondition: TLS credentials are in the storage
InitRetCode Repository::initTlsCreds(const ProvisionConfig& provision_config, const TlsConfig& tls_config) {
  if (loadSetTlsCreds(tls_config)) return INIT_RET_OK;

  if (provision_config.mode != kAutomatic) {
    LOGGER_LOG(LVL_error, "Credentials not found");
    return INIT_RET_STORAGE_FAILURE;
  }

  // Autoprovision is needed and possible => autoprovision

  // set bootstrap credentials
  Bootstrap boot(provision_config.provision_path, provision_config.p12_password);
  http.setCerts(boot.getCa(), kFile, boot.getCert(), kFile, boot.getPkey(), kFile);

  Json::Value data;
  std::string device_id;
  if (!storage.loadDeviceId(&device_id)) {
    LOGGER_LOG(LVL_error, "device_id unknown during autoprovisioning process");
    return INIT_RET_STORAGE_FAILURE;
  }
  data["deviceId"] = device_id;
  data["ttl"] = provision_config.expiry_days;
  HttpResponse response = http.post(provision_config.server + "/devices", data);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
      LOGGER_LOG(LVL_error, "Device id" << device_id << "is occupied");
      return INIT_RET_OCCUPIED;
    } else {
      LOGGER_LOG(LVL_error, "Autoprovisioning failed, response: " << response.body);
      return INIT_RET_SERVER_FAILURE;
    }
  }

  std::string pkey;
  std::string cert;
  std::string ca;
  FILE* device_p12 = fmemopen(const_cast<char*>(response.body.c_str()), response.body.size(), "rb");
  if (!Crypto::parseP12(device_p12, "", &pkey, &cert, &ca)) {
    LOGGER_LOG(LVL_error, "Received a malformed P12 package from the server");
    return INIT_RET_BAD_P12;
  }
  fclose(device_p12);
  storage.storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  if (!loadSetTlsCreds(tls_config)) {
    LOGGER_LOG(LVL_info, "Failed to set provisioned credentials");
    return INIT_RET_STORAGE_FAILURE;
  }

  LOGGER_LOG(LVL_info, "Provisioned successfully on Device Gateway");
  return INIT_RET_OK;
}

void Repository::resetTlsCreds() {
  if (config.provision.mode != kImplicit) {
    storage.clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
InitRetCode Repository::initEcuRegister(const UptaneConfig& uptane_config) {
  if (storage.loadEcuRegistered()) return INIT_RET_OK;

  std::string primary_public;

#ifdef BUILD_P11
  if (uptane_config.key_source == kFile) {
    if (!storage.loadPrimaryKeys(&primary_public, NULL)) {
      LOGGER_LOG(LVL_error, "Unable to read primary public key from the storage");
      return INIT_RET_STORAGE_FAILURE;
    }
  } else {  // kPkcs11
    if (!p11.readUptanePublicKey(&primary_public)) {
      LOGGER_LOG(LVL_error, "Unable to read primary public key from the PKCS#11 device");
      return INIT_RET_STORAGE_FAILURE;
    }
  }
#else
  (void)uptane_config;
  if (!storage.loadPrimaryKeys(&primary_public, NULL)) {
    LOGGER_LOG(LVL_error, "Unable to read primary public key from the storage");
    return INIT_RET_STORAGE_FAILURE;
  }
#endif

  std::vector<std::pair<std::string, std::string> > ecu_serials;
  // initEcuSerials should have been called by this point
  if (!storage.loadEcuSerials(&ecu_serials) || ecu_serials.size() < 1) {
    return INIT_RET_STORAGE_FAILURE;
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = ecu_serials[0].first;
  all_ecus["ecus"] = Json::arrayValue;
  {
    Json::Value primary_ecu;
    primary_ecu["hardware_identifier"] = ecu_serials[0].second;
    primary_ecu["ecu_serial"] = ecu_serials[0].first;
    primary_ecu["clientKey"]["keytype"] = "RSA";
    primary_ecu["clientKey"]["keyval"]["public"] = primary_public;
    all_ecus["ecus"].append(primary_ecu);
  }

  std::map<std::string, std::pair<std::string, std::string> >::const_iterator it;
  for (it = secondary_info.begin(); it != secondary_info.end(); it++) {
    Json::Value ecu;
    ecu["hardware_identifier"] = it->second.first;
    ecu["ecu_serial"] = it->first;
    ecu["clientKey"]["keytype"] = "RSA";  // TODO: add getKeyType() to SecondaryInterface
    ecu["clientKey"]["keyval"]["public"] = it->second.second;
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http.post(config.tls.server + "/director/ecus", all_ecus);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() &&
        (resp_code.asString() == "ecu_already_registered" || resp_code.asString() == "device_already_registered")) {
      LOGGER_LOG(LVL_error, "Some ECU is already registered");
      return INIT_RET_OCCUPIED;
    } else {
      LOGGER_LOG(LVL_error, "Error registering device on Uptane, response: " << response.body);
      return INIT_RET_SERVER_FAILURE;
    }
  }
  // do not call storage.storeEcuRegistered(), it will be called from the top-level Init function after the
  // acknowledgement
  LOGGER_LOG(LVL_info, "ECUs have been successfully registered to the server");
  return INIT_RET_OK;
}

// Postcondition: "ECUs registered" flag set in the storage
bool Repository::initialize() {
  for (int i = 0; i < MaxInitializationAttempts; i++) {
    if (!initDeviceId(config.provision, config.uptane, config.tls)) {
      LOGGER_LOG(LVL_error, "Device ID generation failed, abort initialization");
      return false;
    }
    if (!initPrimaryEcuKeys(config.uptane)) {
      LOGGER_LOG(LVL_error, "ECU key generation failed, abort initialization");
      return false;
    }
    if (!initEcuSerials(config.uptane)) {
      LOGGER_LOG(LVL_error, "ECU serial generation failed, abort initialization");
      return false;
    }

    InitRetCode ret_code = initTlsCreds(config.provision, config.tls);
    // if a device with the same ID has already been registered to the server, repeat the whole registration process
    if (ret_code == INIT_RET_OCCUPIED) {
      resetEcuKeys();
      resetEcuSerials();
      resetDeviceId();
      LOGGER_LOG(LVL_info, "Device name is already registered, restart");
      continue;
    } else if (ret_code != INIT_RET_OK) {
      LOGGER_LOG(LVL_error, "Autoprovisioning failed, abort initialization");
      return false;
    }

    ret_code = initEcuRegister(config.uptane);
    // if am ECU with the same ID has already been registered to the server, repeat the whole registration process
    //   excluding the device registration
    if (ret_code == INIT_RET_OCCUPIED) {
      LOGGER_LOG(LVL_info, "ECU serial is already registered");
    } else if (ret_code != INIT_RET_OK) {
      LOGGER_LOG(LVL_error, "ECU registration failed, abort initialization");
      return false;
    }

    // TODO: acknowledge on server _before_ setting the flag
    storage.storeEcuRegistered();
    return true;
  }
  LOGGER_LOG(LVL_error, "Initialization failed after " << MaxInitializationAttempts << " attempts");
  return false;
}

void Repository::initReset() {
  storage.clearEcuRegistered();
  resetTlsCreds();
  resetEcuKeys();
  resetEcuSerials();
  resetDeviceId();
}
}
