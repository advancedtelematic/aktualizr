#ifndef CRYPTOKEY_H_
#define CRYPTOKEY_H_

#include <boost/move/unique_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "httpinterface.h"
#include "invstorage.h"
#ifdef BUILD_P11
#include "p11engine.h"
#endif
#include "utils.h"

class CryptoKey {
 public:
  // std::string RSAPSSSign(const std::string &message);
  // Contains the logic from HttpClient::setCerts()
  void copyCertsToCurl(HttpInterface *http);
  CryptoKey(const boost::shared_ptr<INvStorage> &backend, const Config &config);
  std::string getPkeyFile();
  std::string getCertFile();
  std::string getCaFile();
  std::string getPkey() const;
  std::string getCert() const;
  std::string getCa() const;
  std::string getCN() const;
  bool isOk() const { return (getPkey().size() && getCert().size() && getCa().size()); }
  std::string generateUptaneKeyPair();
  std::string getUptanePublicKey();
  Json::Value signTuf(const Json::Value &in_data);

 private:
  const boost::shared_ptr<INvStorage> &backend_;
  Config config_;
#ifdef BUILD_P11
  P11EngineGuard p11_;
#endif
  boost::movelib::unique_ptr<TemporaryFile> tmp_pkey_file;
  boost::movelib::unique_ptr<TemporaryFile> tmp_cert_file;
  boost::movelib::unique_ptr<TemporaryFile> tmp_ca_file;
};

#endif  // CRYPTOKEY_H_
