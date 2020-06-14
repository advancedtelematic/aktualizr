#ifndef CRYPTO_KEYMANAGER_CONFIG_H_
#define CRYPTO_KEYMANAGER_CONFIG_H_

#include <libaktualizr/p11_config.h>
#include <libaktualizr/types.h>

// bundle some parts of the main config together
// Should be derived by calling Config::keymanagerConfig()
struct KeyManagerConfig {
  KeyManagerConfig() = delete;  // only allow construction by initializer list
  P11Config p11;
  CryptoSource tls_ca_source;
  CryptoSource tls_pkey_source;
  CryptoSource tls_cert_source;
  KeyType uptane_key_type;
  CryptoSource uptane_key_source;
};

#endif  // CRYPTO_KEYMANAGER_CONFIG_H_
