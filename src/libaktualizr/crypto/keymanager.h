#ifndef KEYMANAGER_H_
#define KEYMANAGER_H_

#include "libaktualizr/config.h"

#include "crypto.h"
#include "http/httpinterface.h"
#include "p11engine.h"

class INvStorage;

class KeyManager {
 public:
  // std::string RSAPSSSign(const std::string &message);
  // Contains the logic from HttpClient::setCerts()
  void copyCertsToCurl(HttpInterface &http) const;
  KeyManager(std::shared_ptr<INvStorage> backend, KeyManagerConfig config);
  void loadKeys(const std::string *pkey_content = nullptr, const std::string *cert_content = nullptr,
                const std::string *ca_content = nullptr);
  std::string getPkeyFile() const;
  std::string getCertFile() const;
  std::string getCaFile() const;
  std::string getPkey() const;
  std::string getCert() const;
  std::string getCa() const;
  std::string getCN() const;
  void getCertInfo(std::string *subject, std::string *issuer, std::string *not_before, std::string *not_after) const;
  bool isOk() const { return ((getPkey().size() != 0U) && (getCert().size() != 0U) && (getCa().size() != 0U)); }
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
