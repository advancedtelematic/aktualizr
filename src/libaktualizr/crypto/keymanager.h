#ifndef KEYMANAGER_H_
#define KEYMANAGER_H_

#include "crypto/p11engine.h"
#include "http/httpinterface.h"
#include "storage/invstorage.h"
#include "utilities/types.h"
#include "utilities/utils.h"

// bundle some parts of the main config together
// Should be derived by calling Config::keymanagerConfig()
struct KeyManagerConfig {
  P11Config p11;
  CryptoSource tls_ca_source;
  CryptoSource tls_pkey_source;
  CryptoSource tls_cert_source;
  KeyType uptane_key_type;
  CryptoSource uptane_key_source;
};

class KeyManager {
 public:
  // std::string RSAPSSSign(const std::string &message);
  // Contains the logic from HttpClient::setCerts()
  void copyCertsToCurl(HttpInterface *http);
  KeyManager(const std::shared_ptr<INvStorage> &backend, KeyManagerConfig config);
  void loadKeys(const std::string *pkey_content = nullptr, const std::string *cert_content = nullptr,
                const std::string *ca_content = nullptr);
  std::string getPkeyFile() const;
  std::string getCertFile() const;
  std::string getCaFile() const;
  std::string getPkey() const;
  std::string getCert() const;
  std::string getCa() const;
  std::string getCN() const;
  bool isOk() const { return ((getPkey().size() != 0u) && (getCert().size() != 0u) && (getCa().size() != 0u)); }
  std::string generateUptaneKeyPair();
  std::string getUptanePublicKey() const;
  Json::Value signTuf(const Json::Value &in_data) const;

 private:
  const std::shared_ptr<INvStorage> &backend_;
  KeyManagerConfig config_;
#ifdef BUILD_P11
  P11EngineGuard p11_;
#endif
  std::unique_ptr<TemporaryFile> tmp_pkey_file;
  std::unique_ptr<TemporaryFile> tmp_cert_file;
  std::unique_ptr<TemporaryFile> tmp_ca_file;
};

#endif  // KEYMANAGER_H_
