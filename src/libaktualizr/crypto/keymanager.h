#ifndef KEYMANAGER_H_
#define KEYMANAGER_H_

#include "crypto.h"
#include "http/httpinterface.h"
#include "keymanager_config.h"
#include "p11engine.h"
#include "utilities/utils.h"

class INvStorage;

class KeyManager {
 public:
  KeyManager(std::shared_ptr<INvStorage> backend, KeyManagerConfig config);
  KeyManager(const KeyManager &) = delete;
  KeyManager &operator=(const KeyManager &) = delete;
  // std::string RSAPSSSign(const std::string &message);
  // Contains the logic from HttpClient::setCerts()
  void copyCertsToCurl(HttpInterface &http);
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
  KeyType getUptaneKeyType() const { return config_.uptane_key_type; }
  Json::Value signTuf(const Json::Value &in_data) const;

  PublicKey UptanePublicKey() const;

 private:
  std::shared_ptr<INvStorage> backend_;
  const KeyManagerConfig config_;
  std::unique_ptr<P11EngineGuard> p11_;
  std::unique_ptr<TemporaryFile> tmp_pkey_file;
  std::unique_ptr<TemporaryFile> tmp_cert_file;
  std::unique_ptr<TemporaryFile> tmp_ca_file;
};

#endif  // KEYMANAGER_H_
