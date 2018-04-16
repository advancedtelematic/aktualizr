#include "utilities/crypto.h"

#include <boost/algorithm/hex.hpp>
#include <boost/scoped_array.hpp>
#include <iostream>

#include <sodium.h>

#include "logging.h"
#include "openssl_compat.h"
#include "utilities/utils.h"

std::string PublicKey::TypeString() const {
  switch (type) {
    case RSA:
      return "rsa";
    case ED25519:
      return "ed25519";
    default:
      return "UNKNOWN";
  }
}

std::string Crypto::sha256digest(const std::string &text) {
  unsigned char sha256_hash[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(sha256_hash, reinterpret_cast<const unsigned char *>(text.c_str()), text.size());
  return std::string(reinterpret_cast<char *>(sha256_hash), crypto_hash_sha256_BYTES);
}

std::string Crypto::sha512digest(const std::string &text) {
  unsigned char sha512_hash[crypto_hash_sha512_BYTES];
  crypto_hash_sha512(sha512_hash, reinterpret_cast<const unsigned char *>(text.c_str()), text.size());
  return std::string(reinterpret_cast<char *>(sha512_hash), crypto_hash_sha512_BYTES);
}

std::string Crypto::RSAPSSSign(ENGINE *engine, const std::string &private_key, const std::string &message) {
  EVP_PKEY *key;
  RSA *rsa = nullptr;
  if (engine != nullptr) {
    key = ENGINE_load_private_key(engine, private_key.c_str(), nullptr, nullptr);

    if (key == nullptr) {
      LOG_ERROR << "ENGINE_load_private_key failed with error " << ERR_error_string(ERR_get_error(), nullptr);
      return std::string();
    }

    rsa = EVP_PKEY_get1_RSA(key);
    EVP_PKEY_free(key);
    if (rsa == nullptr) {
      LOG_ERROR << "EVP_PKEY_get1_RSA failed with error " << ERR_error_string(ERR_get_error(), nullptr);
      return std::string();
    }
  } else {
    BIO *bio = BIO_new_mem_buf(const_cast<char *>(private_key.c_str()), static_cast<int>(private_key.size()));
    if ((key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr)) != nullptr) {
      rsa = EVP_PKEY_get1_RSA(key);
    }
    BIO_free_all(bio);
    EVP_PKEY_free(key);

    if (rsa == nullptr) {
      LOG_ERROR << "PEM_read_bio_PrivateKey failed with error " << ERR_error_string(ERR_get_error(), nullptr);
      return std::string();
    }

#if AKTUALIZR_OPENSSL_PRE_11
    RSA_set_method(rsa, RSA_PKCS1_SSLeay());
#else
    RSA_set_method(rsa, RSA_PKCS1_OpenSSL());
#endif
  }

  const unsigned int sign_size = RSA_size(rsa);
  boost::scoped_array<unsigned char> EM(new unsigned char[sign_size]);
  boost::scoped_array<unsigned char> pSignature(new unsigned char[sign_size]);

  std::string digest = Crypto::sha256digest(message);
  int status = RSA_padding_add_PKCS1_PSS(rsa, EM.get(), reinterpret_cast<const unsigned char *>(digest.c_str()),
                                         EVP_sha256(), -1 /* maximum salt length*/);
  if (status == 0) {
    LOG_ERROR << "RSA_padding_add_PKCS1_PSS failed with error " << ERR_error_string(ERR_get_error(), nullptr);
    RSA_free(rsa);
    return std::string();
  }

  /* perform digital signature */
  status = RSA_private_encrypt(RSA_size(rsa), EM.get(), pSignature.get(), rsa, RSA_NO_PADDING);
  if (status == -1) {
    LOG_ERROR << "RSA_private_encrypt failed with error " << ERR_error_string(ERR_get_error(), nullptr);
    RSA_free(rsa);
    return std::string();
  }
  std::string retval = std::string(reinterpret_cast<char *>(pSignature.get()), sign_size);
  RSA_free(rsa);
  return retval;
}

std::string Crypto::Sign(KeyType key_type, ENGINE *engine, const std::string &private_key, const std::string &message) {
  if (key_type == kED25519) {
    return Crypto::ED25519Sign(boost::algorithm::unhex(private_key), message);
  }
  return Crypto::RSAPSSSign(engine, private_key, message);
}

std::string Crypto::ED25519Sign(const std::string &private_key, const std::string &message) {
  unsigned char sig[crypto_sign_BYTES];
  crypto_sign_detached(sig, nullptr, reinterpret_cast<const unsigned char *>(message.c_str()), message.size(),
                       reinterpret_cast<const unsigned char *>(private_key.c_str()));
  return std::string(reinterpret_cast<char *>(sig), crypto_sign_BYTES);
}

std::string Crypto::getKeyId(const std::string &key) {
  std::string key_content = key;

  boost::algorithm::trim_right_if(key_content, boost::algorithm::is_any_of("\n"));
  std::string keyid = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(Json::Value(key_content))));
  std::transform(keyid.begin(), keyid.end(), keyid.begin(), ::tolower);
  return keyid;
}

bool Crypto::RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message) {
  RSA *rsa = nullptr;
  BIO *bio = BIO_new_mem_buf(const_cast<char *>(public_key.c_str()), static_cast<int>(public_key.size()));
  if (PEM_read_bio_RSA_PUBKEY(bio, &rsa, nullptr, nullptr) == nullptr) {
    LOG_ERROR << "PEM_read_bio_RSA_PUBKEY failed with error " << ERR_error_string(ERR_get_error(), nullptr);
    BIO_free_all(bio);
    return false;
  }
  BIO_free_all(bio);

#if AKTUALIZR_OPENSSL_PRE_11
  RSA_set_method(rsa, RSA_PKCS1_SSLeay());
#else
  RSA_set_method(rsa, RSA_PKCS1_OpenSSL());
#endif

  const unsigned int size = RSA_size(rsa);
  boost::scoped_array<unsigned char> pDecrypted(new unsigned char[size]);
  /* now we will verify the signature
    Start by a RAW decrypt of the signature
  */
  int status =
      RSA_public_decrypt(static_cast<int>(signature.size()), reinterpret_cast<const unsigned char *>(signature.c_str()),
                         pDecrypted.get(), rsa, RSA_NO_PADDING);
  if (status == -1) {
    LOG_ERROR << "RSA_public_decrypt failed with error " << ERR_error_string(ERR_get_error(), nullptr);
    RSA_free(rsa);
    return false;
  }

  std::string digest = Crypto::sha256digest(message);

  /* verify the data */
  status = RSA_verify_PKCS1_PSS(rsa, reinterpret_cast<const unsigned char *>(digest.c_str()), EVP_sha256(),
                                pDecrypted.get(), -2 /* salt length recovered from signature*/);
  RSA_free(rsa);
  return status == 1;
}
bool Crypto::ED25519Verify(const std::string &public_key, const std::string &signature, const std::string &message) {
  return crypto_sign_verify_detached(reinterpret_cast<const unsigned char *>(signature.c_str()),
                                     reinterpret_cast<const unsigned char *>(message.c_str()), message.size(),
                                     reinterpret_cast<const unsigned char *>(public_key.c_str())) == 0;
}

bool Crypto::VerifySignature(const PublicKey &public_key, const std::string &signature, const std::string &message) {
  if (public_key.type == PublicKey::ED25519) {
    return ED25519Verify(boost::algorithm::unhex(public_key.value), Utils::fromBase64(signature), message);
  }
  if (public_key.type == PublicKey::RSA) {
    return RSAPSSVerify(public_key.value, Utils::fromBase64(signature), message);
  }
  LOG_ERROR << "unsupported keytype: " << public_key.type;
  return false;
}

bool Crypto::parseP12(BIO *p12_bio, const std::string &p12_password, std::string *out_pkey, std::string *out_cert,
                      std::string *out_ca) {
#if AKTUALIZR_OPENSSL_PRE_11
  SSLeay_add_all_algorithms();
#endif
  PKCS12 *p12 = d2i_PKCS12_bio(p12_bio, nullptr);
  if (p12 == nullptr) {
    LOG_ERROR << "Could not read from " << p12_bio << " file pointer";
    return false;
  }

  EVP_PKEY *pkey = nullptr;
  X509 *x509_cert = nullptr;
  STACK_OF(X509) *ca_certs = nullptr;
  if (PKCS12_parse(p12, p12_password.c_str(), &pkey, &x509_cert, &ca_certs) == 0) {
    LOG_ERROR << "Could not parse file from " << p12_bio << " source pointer";
    PKCS12_free(p12);
    return false;
  }
  PKCS12_free(p12);

  BIO *pkey_pem_sink = BIO_new(BIO_s_mem());
  if (pkey_pem_sink == nullptr) {
    LOG_ERROR << "Could not open pkey buffer for writing";
    EVP_PKEY_free(pkey);
    return false;
  }
  PEM_write_bio_PrivateKey(pkey_pem_sink, pkey, nullptr, nullptr, 0, nullptr, nullptr);
  EVP_PKEY_free(pkey);
  char *pkey_buf;
  auto pkey_len = BIO_get_mem_data(pkey_pem_sink, &pkey_buf);  // NOLINT
  *out_pkey = std::string(pkey_buf, pkey_len);
  BIO_free(pkey_pem_sink);

  char *cert_buf;
  size_t cert_len;
  BIO *cert_sink = BIO_new(BIO_s_mem());
  if (cert_sink == nullptr) {
    LOG_ERROR << "Could not open certificate buffer for writing";
    return false;
  }
  PEM_write_bio_X509(cert_sink, x509_cert);

  char *ca_buf;
  size_t ca_len;
  BIO *ca_sink = BIO_new(BIO_s_mem());
  if (ca_sink == nullptr) {
    LOG_ERROR << "Could not open ca buffer for writing";
    return false;
  }
  X509 *ca_cert = nullptr;
  for (int i = 0; i < sk_X509_num(ca_certs); i++) {
    ca_cert = sk_X509_value(ca_certs, i);
    PEM_write_bio_X509(ca_sink, ca_cert);
    PEM_write_bio_X509(cert_sink, ca_cert);
  }
  ca_len = BIO_get_mem_data(ca_sink, &ca_buf);  // NOLINT
  *out_ca = std::string(ca_buf, ca_len);
  BIO_free(ca_sink);

  cert_len = BIO_get_mem_data(cert_sink, &cert_buf);  // NOLINT
  *out_cert = std::string(cert_buf, cert_len);
  BIO_free(cert_sink);

  sk_X509_pop_free(ca_certs, X509_free);
  X509_free(x509_cert);

  return true;
}

bool Crypto::extractSubjectCN(const std::string &cert, std::string *cn) {
  BIO *bio = BIO_new_mem_buf(const_cast<char *>(cert.c_str()), static_cast<int>(cert.size()));
  X509 *x = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free_all(bio);
  if (x == nullptr) {
    return false;
  }

  int len = X509_NAME_get_text_by_NID(X509_get_subject_name(x), NID_commonName, nullptr, 0);
  if (len < 0) {
    X509_free(x);
    return false;
  }
  boost::scoped_array<char> buf(new char[len + 1]);
  X509_NAME_get_text_by_NID(X509_get_subject_name(x), NID_commonName, buf.get(), len + 1);
  *cn = std::string(buf.get());
  X509_free(x);
  return true;
}

/**
 * Generate a RSA keypair
 * @param public_key Generated public part of key
 * @param private_key Generated private part of key
 * @return true if the keys are present at the end of this function (either they were created or existed already)
 *         false if key generation failed
 */
bool Crypto::generateRSAKeyPair(KeyType key_type, std::string *public_key, std::string *private_key) {
  int bits = (key_type == kRSA2048) ? 2048 : 4096;
  int ret = 0;

#if AKTUALIZR_OPENSSL_PRE_11
  RSA *r = RSA_generate_key(bits,    /* number of bits for the key - 2048 is a sensible value */
                            RSA_F4,  /* exponent - RSA_F4 is defined as 0x10001L */
                            nullptr, /* callback - can be NULL if we aren't displaying progress */
                            nullptr  /* callback argument - not needed in this case */
                            );
#else
  BIGNUM *bne = BN_new();
  ret = BN_set_word(bne, RSA_F4);
  if (ret != 1) {
    BN_free(bne);
    return false;
  }
  RSA *r = RSA_new();
  ret = RSA_generate_key_ex(r, bits, /* number of bits for the key - 2048 is a sensible value */
                            bne,     /* exponent - RSA_F4 is defined as 0x10001L */
                            nullptr  /* callback argument - not needed in this case */
                            );
  if (ret != 1) {
    RSA_free(r);
    BN_free(bne);
    return false;
  }
#endif

  EVP_PKEY *pkey = EVP_PKEY_new();
  EVP_PKEY_assign_RSA(pkey, r);  // NOLINT
  char *pubkey_buf;
  BIO *pubkey_sink = BIO_new(BIO_s_mem());
  if (pubkey_sink == nullptr) {
    return false;
  }
  ret = PEM_write_bio_PUBKEY(pubkey_sink, pkey);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    BIO_free(pubkey_sink);
    return false;
  }
  auto pubkey_len = BIO_get_mem_data(pubkey_sink, &pubkey_buf);  // NOLINT
  *public_key = std::string(pubkey_buf, pubkey_len);
  BIO_free(pubkey_sink);

  char *privkey_buf;
  BIO *privkey_sink = BIO_new(BIO_s_mem());
  if (privkey_sink == nullptr) {
    return false;
  }

  ret = PEM_write_bio_RSAPrivateKey(privkey_sink, r, nullptr, nullptr, 0, nullptr, nullptr);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    BIO_free(privkey_sink);
    return false;
  }
  auto privkey_len = BIO_get_mem_data(privkey_sink, &privkey_buf);  // NOLINT
  *private_key = std::string(privkey_buf, privkey_len);
  BIO_free(privkey_sink);
  EVP_PKEY_free(pkey);
#if AKTUALIZR_OPENSSL_AFTER_11
  BN_free(bne);
#endif
  return true;
}

bool Crypto::generateEDKeyPair(std::string *public_key, std::string *private_key) {
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  crypto_sign_keypair(pk, sk);
  *public_key = boost::algorithm::hex(std::string(reinterpret_cast<char *>(pk), crypto_sign_PUBLICKEYBYTES));
  // std::transform(public_key->begin(), public_key->end(), public_key->begin(), ::tolower);
  *private_key = boost::algorithm::hex(std::string(reinterpret_cast<char *>(sk), crypto_sign_SECRETKEYBYTES));
  // std::transform(private_key->begin(), private_key->end(), private_key->begin(), ::tolower);
  return true;
}

bool Crypto::generateKeyPair(KeyType key_type, std::string *public_key, std::string *private_key) {
  if (key_type == kED25519) {
    return Crypto::generateEDKeyPair(public_key, private_key);
  }
  return Crypto::generateRSAKeyPair(key_type, public_key, private_key);
}
