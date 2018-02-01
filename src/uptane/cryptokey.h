#ifndef CRYPTOKEY_H_
#define CRYPTOKEY_H_

#include "invstorage.h"
#include "config.h"
#include "httpinterface.h"
#ifdef BUILD_P11
#include "p11engine.h"
#endif
#include <boost/shared_ptr.hpp>



class CryptoKey {
  public:
    //std::string RSAPSSSign(const std::string &message);
    // Contains the logic from HttpClient::setCerts()
    void copyCertsToCurl(HttpInterface* http);
    CryptoKey(const boost::shared_ptr<INvStorage> &backend, const Config& config);
    std::string getPkey() const;
    std::string getCert() const;
    std::string getCa() const;
    std::string getCN() const;
    bool isOk() const {return (getPkey().size() && getCert().size() && getCa().size());}
    std::string generateUptaneKeyPair();
    std::string getUptanePublicKey();
    Json::Value signTuf(const Json::Value &in_data);
  private:
    boost::shared_ptr<INvStorage> backend_;
    Config config_;
#ifdef BUILD_P11
    P11Engine& p11_;
#endif

};

#endif // CRYPTOKEY_H_
