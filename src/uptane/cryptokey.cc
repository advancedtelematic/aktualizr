#include "cryptokey.h"

#include <boost/scoped_array.hpp>
#include <stdexcept>

CryptoKey::CryptoKey(const boost::shared_ptr<INvStorage> &backend, const Config &config)
    : backend_(backend),
      config_(config)
#ifdef BUILD_P11
      ,
      p11_(P11Engine::Get(config.p11))
#endif
{
}

std::string CryptoKey::getPkey() const {
  std::string pkey;
#ifdef BUILD_P11
  if (config_.tls.pkey_source == kPkcs11) {
    pkey = p11_->getTlsPkeyId();
  }
#endif
  if (config_.tls.pkey_source == kFile) {
    backend_->loadTlsPkey(&pkey);
  }
  return pkey;
}

std::string CryptoKey::getCert() const {
  std::string cert;
#ifdef BUILD_P11
  if (config_.tls.cert_source == kPkcs11) {
    cert = p11_->getTlsCertId();
  }
#endif
  if (config_.tls.cert_source == kFile) {
    backend_->loadTlsCert(&cert);
  }
  return cert;
}

std::string CryptoKey::getCa() const {
  std::string ca;
#ifdef BUILD_P11
  if (config_.tls.ca_source == kPkcs11) {
    ca = p11_->getTlsCacertId();
  }
#endif
  if (config_.tls.ca_source == kFile) {
    backend_->loadTlsCa(&ca);
  }
  return ca;
}

std::string CryptoKey::getCN() const {
  std::string not_found_cert_message = "Certificate is not found, can't extract device_id";
  std::string cert;
  if (config_.tls.cert_source == kFile) {
    if (!backend_->loadTlsCert(&cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
  } else {  // kPkcs11
#ifdef BUILD_P11
    if (!p11_->readTlsCert(&cert)) {
      throw std::runtime_error(not_found_cert_message);
    }
#else
    throw std::runtime_error("Aktualizr was built without PKCS#11 support, can't extract device_id");
#endif
  }

  BIO *bio = BIO_new_mem_buf(const_cast<char *>(cert.c_str()), (int)cert.size());
  X509 *x = PEM_read_bio_X509(bio, NULL, 0, NULL);
  BIO_free_all(bio);
  if (!x) throw std::runtime_error("Could not parse certificate");

  int len = X509_NAME_get_text_by_NID(X509_get_subject_name(x), NID_commonName, NULL, 0);
  if (len < 0) {
    X509_free(x);
    throw std::runtime_error("Could not get CN from certificate");
  }
  boost::scoped_array<char> buf(new char[len + 1]);
  X509_NAME_get_text_by_NID(X509_get_subject_name(x), NID_commonName, buf.get(), len + 1);
  std::string cn(buf.get());
  X509_free(x);
  return cn;
}

void CryptoKey::copyCertsToCurl(HttpInterface *http) {
  std::string pkey = getPkey();
  std::string cert = getCert();
  std::string ca = getCa();

  if (pkey.size() && cert.size() && ca.size()) {
    http->setCerts(ca, config_.tls.ca_source, cert, config_.tls.cert_source, pkey, config_.tls.pkey_source);
  }
}

Json::Value CryptoKey::signTuf(const Json::Value &in_data) {
  ENGINE *crypto_engine = NULL;
  std::string private_key;
#ifdef BUILD_P11
  if (config_.uptane.key_source == kPkcs11) crypto_engine = p11_->getEngine();
#endif
  if (config_.uptane.key_source == kFile) {
    backend_->loadPrimaryPrivate(&private_key);
  }
  std::string b64sig =
      Utils::toBase64(Crypto::RSAPSSSign(crypto_engine, private_key, Json::FastWriter().write(in_data)));
  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  Json::Value out_data;
  signature["keyid"] = Crypto::getKeyId(getUptanePublicKey());
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return out_data;
}

std::string CryptoKey::getUptanePublicKey() {
  std::string primary_public;
  if (config_.uptane.key_source == kFile) {
    if (!backend_->loadPrimaryPublic(&primary_public)) {
      throw std::runtime_error("Could not get uptane public key!");
    }
  } else {
#ifdef BUILD_P11
    // dummy read to check if the key is present
    if (!p11_->readUptanePublicKey(&primary_public)) {
      throw std::runtime_error("Could not get uptane public key!");
    }
#else
    throw std::runtime_error("Aktualizr builded without pkcs11 support!");
#endif
  }
  return primary_public;
}

std::string CryptoKey::generateUptaneKeyPair() {
  std::string primary_public;

  if (config_.uptane.key_source == kFile) {
    std::string primary_private;
    if (!backend_->loadPrimaryKeys(&primary_public, &primary_private)) {
      if (Crypto::generateRSAKeyPair(&primary_public, &primary_private)) {
        backend_->storePrimaryKeys(primary_public, primary_private);
      }
    }
    if (primary_public.empty() && primary_private.empty()) {
      throw std::runtime_error("Could not get uptane keys");
    }
  } else {
#ifdef BUILD_P11
    // dummy read to check if the key is present
    if (!p11_->readUptanePublicKey(&primary_public)) {
      p11_->generateUptaneKeyPair();
    }
    // realy read the key
    if (primary_public.empty() && !p11_->readUptanePublicKey(&primary_public)) {
      throw std::runtime_error("Could not get uptane keys");
    }
    return primary_public;
#else
    throw std::runtime_error("Aktualizr builded without pkcs11 support!");
#endif
  }
  return primary_public;
}
