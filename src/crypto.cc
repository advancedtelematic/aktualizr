#include "crypto.h"

#include <boost/algorithm/hex.hpp>
#include <boost/scoped_array.hpp>
#include <iostream>

#include <sodium.h>

#include "logger.h"
#include "openssl_compat.h"
#include "utils.h"

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
  crypto_hash_sha256(sha256_hash, (const unsigned char *)text.c_str(), text.size());
  return std::string((char *)sha256_hash, crypto_hash_sha256_BYTES);
}

std::string Crypto::sha512digest(const std::string &text) {
  unsigned char sha512_hash[crypto_hash_sha512_BYTES];
  crypto_hash_sha512(sha512_hash, (const unsigned char *)text.c_str(), text.size());
  return std::string((char *)sha512_hash, crypto_hash_sha512_BYTES);
}

std::string Crypto::RSAPSSSign(const std::string &private_key, const std::string &message) {
  EVP_PKEY *key;
  RSA *rsa = NULL;

  FILE *priv_key_file = fopen(private_key.c_str(), "rt");
  if (!priv_key_file) {
    LOGGER_LOG(LVL_error, "error opening " << private_key);
    return std::string();
  }
  if ((key = PEM_read_PrivateKey(priv_key_file, NULL, NULL, NULL))) {
    rsa = EVP_PKEY_get1_RSA(key);
  }
  fclose(priv_key_file);
  EVP_PKEY_free(key);

  if (!rsa) {
    LOGGER_LOG(LVL_error, "PEM_read_PrivateKey failed with error " << ERR_error_string(ERR_get_error(), NULL));
    return std::string();
  }
  const unsigned int sign_size = RSA_size(rsa);
  boost::scoped_array<unsigned char> EM(new unsigned char[sign_size]);
  boost::scoped_array<unsigned char> pSignature(new unsigned char[sign_size]);

  std::string digest = Crypto::sha256digest(message);
  int status = RSA_padding_add_PKCS1_PSS(rsa, EM.get(), (const unsigned char *)digest.c_str(), EVP_sha256(),
                                         -1 /* maximum salt length*/);
  if (!status) {
    LOGGER_LOG(LVL_error, "RSA_padding_add_PKCS1_PSS failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return std::string();
  }

  /* perform digital signature */
  status = RSA_private_encrypt(RSA_size(rsa), EM.get(), pSignature.get(), rsa, RSA_NO_PADDING);
  if (status == -1) {
    LOGGER_LOG(LVL_error, "RSA_private_encrypt failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return std::string();
  }
  std::string retval = std::string((char *)(pSignature.get()), sign_size);
  RSA_free(rsa);
  return retval;
}

Json::Value Crypto::signTuf(const std::string &private_key_path, const std::string &public_key_path,
                            const Json::Value &in_data) {
  std::string b64sig = Utils::toBase64(Crypto::RSAPSSSign(private_key_path, Json::FastWriter().write(in_data)));
  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  std::string key_content(Utils::readFile(public_key_path));
  boost::algorithm::trim_right_if(key_content, boost::algorithm::is_any_of("\n"));
  std::string keyid = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(Json::Value(key_content))));
  std::transform(keyid.begin(), keyid.end(), keyid.begin(), ::tolower);
  Json::Value out_data;
  signature["keyid"] = keyid;
  out_data["signed"] = in_data;
  out_data["signatures"] = Json::Value(Json::arrayValue);
  out_data["signatures"].append(signature);
  return out_data;
}

bool Crypto::RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message) {
  RSA *rsa = NULL;

  BIO *bio = BIO_new_mem_buf(const_cast<char *>(public_key.c_str()), (int)public_key.size());
  if (!PEM_read_bio_RSA_PUBKEY(bio, &rsa, NULL, NULL)) {
    LOGGER_LOG(LVL_error, "PEM_read_bio_RSA_PUBKEY failed with error " << ERR_error_string(ERR_get_error(), NULL));
    BIO_free_all(bio);
    return false;
  }
  BIO_free_all(bio);

  const unsigned int size = RSA_size(rsa);
  boost::scoped_array<unsigned char> pDecrypted(new unsigned char[size]);
  /* now we will verify the signature
    Start by a RAW decrypt of the signature
  */
  int status = RSA_public_decrypt((int)signature.size(), (const unsigned char *)signature.c_str(), pDecrypted.get(),
                                  rsa, RSA_NO_PADDING);
  if (status == -1) {
    LOGGER_LOG(LVL_error, "RSA_public_decrypt failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return false;
  }

  std::string digest = Crypto::sha256digest(message);

  /* verify the data */
  status = RSA_verify_PKCS1_PSS(rsa, (const unsigned char *)digest.c_str(), EVP_sha256(), pDecrypted.get(),
                                -2 /* salt length recovered from signature*/);
  RSA_free(rsa);
  if (status == 1) {
    return true;
  }
  return false;
}
bool Crypto::ED25519Verify(const std::string &public_key, const std::string &signature, const std::string &message) {
  return crypto_sign_verify_detached((const unsigned char *)signature.c_str(), (const unsigned char *)message.c_str(),
                                     message.size(), (const unsigned char *)public_key.c_str()) == 0;
}

bool Crypto::VerifySignature(const PublicKey &public_key, const std::string &signature, const std::string &message) {
  if (public_key.type == PublicKey::ED25519) {
    return ED25519Verify(boost::algorithm::unhex(public_key.value), Utils::fromBase64(signature), message);
  } else if (public_key.type == PublicKey::RSA) {
    return RSAPSSVerify(public_key.value, Utils::fromBase64(signature), message);
  } else {
    LOGGER_LOG(LVL_error, "unsupported keytype: " << public_key.type);
    return false;
  }
}

bool Crypto::parseP12(FILE *p12_fp, const std::string &p12_password, std::string *out_pkey, std::string *out_cert,
                      std::string *out_ca) {
#if AKTUALIZR_OPENSSL_PRE_11
  SSLeay_add_all_algorithms();
#endif
  PKCS12 *p12 = d2i_PKCS12_fp(p12_fp, NULL);
  if (!p12) {
    LOGGER_LOG(LVL_error, "Could not read from " << p12_fp << " file pointer");
    return false;
  }

  EVP_PKEY *pkey = NULL;
  X509 *x509_cert = NULL;
  STACK_OF(X509) *ca_certs = NULL;
  if (!PKCS12_parse(p12, p12_password.c_str(), &pkey, &x509_cert, &ca_certs)) {
    LOGGER_LOG(LVL_error, "Could not parse file from " << p12_fp << " file pointer");
    PKCS12_free(p12);
    return false;
  }
  PKCS12_free(p12);

  char *pkey_buf;
  size_t pkey_len;
  FILE *pkey_pem_file = open_memstream(&pkey_buf, &pkey_len);
  if (!pkey_pem_file) {
    LOGGER_LOG(LVL_error, "Could not open pkey buffer for writting");
    EVP_PKEY_free(pkey);
    return false;
  }
  PEM_write_PrivateKey(pkey_pem_file, pkey, NULL, NULL, 0, 0, NULL);
  fclose(pkey_pem_file);
  EVP_PKEY_free(pkey);
  *out_pkey = std::string(pkey_buf, pkey_len);
  free(pkey_buf);

  char *cert_buf;
  size_t cert_len;
  FILE *cert_file = open_memstream(&cert_buf, &cert_len);
  if (!cert_file) {
    LOGGER_LOG(LVL_error, "Could not open certificate buffer for writting");
    return false;
  }
  PEM_write_X509(cert_file, x509_cert);

  char *ca_buf;
  size_t ca_len;
  FILE *ca_file = open_memstream(&ca_buf, &ca_len);
  if (!ca_file) {
    LOGGER_LOG(LVL_error, "Could not open ca buffer for writting");
    return false;
  }
  X509 *ca_cert = NULL;
  for (int i = 0; i < sk_X509_num(ca_certs); i++) {
    ca_cert = sk_X509_value(ca_certs, i);
    PEM_write_X509(ca_file, ca_cert);
    PEM_write_X509(cert_file, ca_cert);
  }
  fclose(ca_file);
  *out_ca = std::string(ca_buf, ca_len);
  free(ca_buf);

  fclose(cert_file);
  *out_cert = std::string(cert_buf, cert_len);
  free(cert_buf);

  sk_X509_pop_free(ca_certs, X509_free);
  X509_free(x509_cert);

  return true;
}

/**
 * Generate a RSA keypair
 * @param public_key Generated public part of key
 * @param private_key Generated private part of key
 * @return true if the keys are present at the end of this function (either they were created or existed already)
 *         false if key generation failed
 */
bool Crypto::generateRSAKeyPair(std::string *public_key, std::string *private_key) {
  int bits = 2048;
  int ret = 0;

#if AKTUALIZR_OPENSSL_PRE_11
  RSA *r = RSA_generate_key(bits,   /* number of bits for the key - 2048 is a sensible value */
                            RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
                            NULL,   /* callback - can be NULL if we aren't displaying progress */
                            NULL    /* callback argument - not needed in this case */
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
                            NULL     /* callback argument - not needed in this case */
                            );
  if (ret != 1) {
    RSA_free(r);
    BN_free(bne);
    return false;
  }
#endif

  EVP_PKEY *pkey = EVP_PKEY_new();
  EVP_PKEY_assign_RSA(pkey, r);
  char *pubkey_buf;
  size_t pubkey_len;
  FILE *pubkey_file = open_memstream(&pubkey_buf, &pubkey_len);
  if (!pubkey_file) {
    return false;
  }
  ret = PEM_write_PUBKEY(pubkey_file, pkey);
  fclose(pubkey_file);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    free(pubkey_buf);
    return false;
  }
  *public_key = std::string(pubkey_buf, pubkey_len);
  free(pubkey_buf);

  char *privkey_buf;
  size_t privkey_len;
  FILE *privkey_file = open_memstream(&privkey_buf, &privkey_len);
  if (!privkey_file) {
    return false;
  }

  ret = PEM_write_RSAPrivateKey(privkey_file, r, NULL, NULL, 0, NULL, NULL);
  fclose(privkey_file);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    free(privkey_buf);
    return false;
  }
  *private_key = std::string(privkey_buf, privkey_len);
  free(privkey_buf);
  RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
  BN_free(bne);
#endif
  return true;
}
