#include "utilities/keymanager.h"

#include <stdexcept>

#include <boost/scoped_array.hpp>
#include <utility>

KeyManager::KeyManager(const std::shared_ptr<INvStorage> &backend, KeyManagerConfig config)
    : backend_(backend),
      config_(std::move(config))
#ifdef BUILD_P11
      ,
      p11_(config_.p11)
#endif
{
}

void KeyManager::loadKeys(const std::string *pkey_content, const std::string *cert_content,
                          const std::string *ca_content) {
  if (config_.tls_pkey_source == kFile) {
    std::string pkey;
    if (pkey_content != nullptr) {
      pkey = *pkey_content;
    } else {
      backend_->loadTlsPkey(&pkey);
    }
    if (!pkey.empty()) {
      if (tmp_pkey_file == nullptr) {
        tmp_pkey_file = std_::make_unique<TemporaryFile>("tls-pkey");
      }
      tmp_pkey_file->PutContents(pkey);
    }
  }
  if (config_.tls_cert_source == kFile) {
    std::string cert;
    if (cert_content != nullptr) {
      cert = *cert_content;
    } else {
      backend_->loadTlsCert(&cert);
    }
    if (!cert.empty()) {
      if (tmp_cert_file == nullptr) {
        tmp_cert_file = std_::make_unique<TemporaryFile>("tls-cert");
      }
      tmp_cert_file->PutContents(cert);
    }
  }
  if (config_.tls_ca_source == kFile) {
    std::string ca;
    if (ca_content != nullptr) {
      ca = *ca_content;
    } else {
      backend_->loadTlsCa(&ca);
    }
    if (!ca.empty()) {
      if (tmp_ca_file == nullptr) {
        tmp_ca_file = std_::make_unique<TemporaryFile>("tls-ca");
      }
      tmp_ca_file->PutContents(ca);
    }
  }
}

std::string KeyManager::getPkeyFile() const {
  std::string pkey_file;
#ifdef BUILD_P11
  if (config_.tls_pkey_source == kPkcs11) {
    pkey_file = p11_->getTlsPkeyId();
  }
#endif
  if (config_.tls_pkey_source == kFile) {
    if (tmp_pkey_file && !boost::filesystem::is_empty(tmp_pkey_file->PathString())) {
      pkey_file = tmp_pkey_file->PathString();
    }
  }
  return pkey_file;
}

std::string KeyManager::getCertFile() const {
  std::string cert_file;
#ifdef BUILD_P11
  if (config_.tls_cert_source == kPkcs11) {
    cert_file = p11_->getTlsCertId();
  }
#endif
  if (config_.tls_cert_source == kFile) {
    if (tmp_cert_file && !boost::filesystem::is_empty(tmp_cert_file->PathString())) {
      cert_file = tmp_cert_file->PathString();
    }
  }
  return cert_file;
}

std::string KeyManager::getCaFile() const {
  std::string ca_file;
#ifdef BUILD_P11
  if (config_.tls_ca_source == kPkcs11) {
    ca_file = p11_->getTlsCacertId();
  }
#endif
  if (config_.tls_ca_source == kFile) {
    if (tmp_ca_file && !boost::filesystem::is_empty(tmp_ca_file->PathString())) {
      ca_file = tmp_ca_file->PathString();
    }
  }
  return ca_file;
}

std::string KeyManager::getPkey() const {
  std::string pkey;
#ifdef BUILD_P11
  if (config_.tls_pkey_source == kPkcs11) {
    pkey = p11_->getTlsPkeyId();
  }
#endif
  if (config_.tls_pkey_source == kFile) {
    backend_->loadTlsPkey(&pkey);
  }
  return pkey;
}

std::string KeyManager::getCert() const {
  std::string cert;
#ifdef BUILD_P11
  if (config_.tls_cert_source == kPkcs11) {
    cert = p11_->getTlsCertId();
  }
#endif
  if (config_.tls_cert_source == kFile) {
    backend_->loadTlsCert(&cert);
  }
  return cert;
}

std::string KeyManager::getCa() const {
  std::string ca;
#ifdef BUILD_P11
  if (config_.tls_ca_source == kPkcs11) {
    ca = p11_->getTlsCacertId();
  }
#endif
  if (config_.tls_ca_source == kFile) {
    backend_->loadTlsCa(&ca);
  }
  return ca;
}

std::string KeyManager::getCN() const {
  std::string not_found_cert_message = "Certificate is not found, can't extract device_id";
  std::string cert;
  if (config_.tls_cert_source == kFile) {
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

  StructGuard<BIO> bio(BIO_new_mem_buf(const_cast<char *>(cert.c_str()), static_cast<int>(cert.size())), BIO_vfree);
  StructGuard<X509> x(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
  if (x == nullptr) {
    throw std::runtime_error("Could not parse certificate");
  }

  int len = X509_NAME_get_text_by_NID(X509_get_subject_name(x.get()), NID_commonName, nullptr, 0);
  if (len < 0) {
    throw std::runtime_error("Could not get CN from certificate");
  }
  boost::scoped_array<char> buf(new char[len + 1]);
  X509_NAME_get_text_by_NID(X509_get_subject_name(x.get()), NID_commonName, buf.get(), len + 1);
  std::string cn(buf.get());
  return cn;
}

void KeyManager::copyCertsToCurl(HttpInterface *http) {
  std::string pkey = getPkey();
  std::string cert = getCert();
  std::string ca = getCa();

  if ((pkey.size() != 0u) && (cert.size() != 0u) && (ca.size() != 0u)) {
    http->setCerts(ca, config_.tls_ca_source, cert, config_.tls_cert_source, pkey, config_.tls_pkey_source);
  }
}

Json::Value KeyManager::signTuf(const Json::Value &in_data) const {
  ENGINE *crypto_engine = nullptr;
  std::string private_key;
#ifdef BUILD_P11
  if (config_.uptane_key_source == kPkcs11) {
    crypto_engine = p11_->getEngine();
    private_key = config_.p11.uptane_key_id;
  }
#endif
  if (config_.uptane_key_source == kFile) {
    backend_->loadPrimaryPrivate(&private_key);
  }
  std::string b64sig = Utils::toBase64(
      Crypto::Sign(config_.uptane_key_type, crypto_engine, private_key, Json::FastWriter().write(in_data)));
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

std::string KeyManager::getUptanePublicKey() const {
  std::string primary_public;
  if (config_.uptane_key_source == kFile) {
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
    throw std::runtime_error("Aktualizr was built without pkcs11 support!");
#endif
  }
  return primary_public;
}

std::string KeyManager::generateUptaneKeyPair() {
  std::string primary_public;

  if (config_.uptane_key_source == kFile) {
    std::string primary_private;
    if (!backend_->loadPrimaryKeys(&primary_public, &primary_private)) {
      if (Crypto::generateKeyPair(config_.uptane_key_type, &primary_public, &primary_private)) {
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
    // really read the key
    if (primary_public.empty() && !p11_->readUptanePublicKey(&primary_public)) {
      throw std::runtime_error("Could not get uptane keys");
    }
    return primary_public;
#else
    throw std::runtime_error("Aktualizr was built without pkcs11 support!");
#endif
  }
  return primary_public;
}
