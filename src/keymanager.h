#ifndef KEYMANAGER_H_
#define KEYMANAGER_H_

#include <boost/move/unique_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "httpinterface.h"
#include "invstorage.h"
#ifdef BUILD_P11
#include "p11engine.h"
#endif
#include "utils.h"

// bundle some parts of the main config together
struct KeyManagerConfig {
  explicit KeyManagerConfig(const Config &config) : p11(config.p11), tls(config.tls), uptane(config.uptane) {}
  KeyManagerConfig(const P11Config &p11_conf, const TlsConfig &tls_conf, const UptaneConfig &uptane_conf)
      : p11(p11_conf), tls(tls_conf), uptane(uptane_conf) {}
  const P11Config &p11;
  const TlsConfig &tls;
  const UptaneConfig &uptane;
};

class KeyManager {
 public:
  // std::string RSAPSSSign(const std::string &message);
  // Contains the logic from HttpClient::setCerts()
  void copyCertsToCurl(HttpInterface *http);
  KeyManager(const boost::shared_ptr<INvStorage> &backend, const KeyManagerConfig &config);
  void loadKeys();
  std::string getPkeyFile() const;
  std::string getCertFile() const;
  std::string getCaFile() const;
  std::string getPkey() const;
  std::string getCert() const;
  std::string getCa() const;
  std::string getCN() const;
  bool isOk() const { return (getPkey().size() && getCert().size() && getCa().size()); }
  std::string generateUptaneKeyPair();
  std::string getUptanePublicKey() const;
  Json::Value signTuf(const Json::Value &in_data);

 private:
  const boost::shared_ptr<INvStorage> &backend_;
  KeyManagerConfig config_;
#ifdef BUILD_P11
  P11EngineGuard p11_;
#endif
  boost::movelib::unique_ptr<TemporaryFile> tmp_pkey_file;
  boost::movelib::unique_ptr<TemporaryFile> tmp_cert_file;
  boost::movelib::unique_ptr<TemporaryFile> tmp_ca_file;
};

#endif  // KEYMANAGER_H_
