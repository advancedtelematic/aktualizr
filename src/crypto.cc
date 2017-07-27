#include "crypto.h"

#include <boost/algorithm/hex.hpp>
#include <boost/scoped_array.hpp>
#include <iostream>

#include <sodium.h>

#include "logger.h"
#include "openssl_compat.h"
#include "utils.h"

std::string Crypto::sha256digest(const std::string &text) {
  EVP_MD_CTX *md_ctx;
  md_ctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(md_ctx, EVP_sha256(), NULL);
  EVP_DigestUpdate(md_ctx, (const void *)text.c_str(), text.size());
  unsigned char digest[32];
  unsigned int digest_len = 32;
  EVP_DigestFinal_ex(md_ctx, digest, &digest_len);
  EVP_MD_CTX_destroy(md_ctx);
  return std::string((char *)digest, 32);
}

std::string Crypto::sha512digest(const std::string &text) {
  const unsigned int size = 64;
  EVP_MD_CTX *md_ctx;
  md_ctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(md_ctx, EVP_sha512(), NULL);
  EVP_DigestUpdate(md_ctx, (const void *)text.c_str(), text.size());
  unsigned char digest[size];
  unsigned int digest_len = size;
  EVP_DigestFinal_ex(md_ctx, digest, &digest_len);
  EVP_MD_CTX_destroy(md_ctx);
  return std::string((char *)digest, size);
}

std::string Crypto::RSAPSSSign(const std::string &private_key, const std::string &message) {
  EVP_PKEY *key;
  RSA *rsa = NULL;

  FILE *priv_key_file = fopen(private_key.c_str(), "rt");
  if (!priv_key_file) {
    LOGGER_LOG(LVL_error, "error opening" << private_key);
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

bool Crypto::parseP12(FILE *p12_fp, const std::string &p12_password, const std::string &pkey_pem,
                      const std::string &client_pem, const std::string ca_pem) {
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
    return false;
  }

  FILE *pkey_pem_file = fopen(pkey_pem.c_str(), "w");
  if (!pkey_pem_file) {
    LOGGER_LOG(LVL_error, "Could not open " << pkey_pem << " for writting");
    return false;
  }
  PEM_write_PrivateKey(pkey_pem_file, pkey, NULL, NULL, 0, 0, NULL);
  fclose(pkey_pem_file);

  FILE *cert_file = fopen(client_pem.c_str(), "w");
  if (!cert_file) {
    LOGGER_LOG(LVL_error, "Could not open " << client_pem << " for writting");
    return false;
  }
  PEM_write_X509(cert_file, x509_cert);

  FILE *ca_file = fopen(ca_pem.c_str(), "w");
  if (!ca_file) {
    LOGGER_LOG(LVL_error, "Could not open " << ca_pem << " for writting");
    return false;
  }
  X509 *ca_cert = NULL;
  for (int i = 0; i < sk_X509_num(ca_certs); i++) {
    ca_cert = sk_X509_value(ca_certs, i);
    PEM_write_X509(ca_file, ca_cert);
    PEM_write_X509(cert_file, ca_cert);
  }
  fclose(ca_file);
  fclose(cert_file);
  sk_X509_pop_free(ca_certs, X509_free);
  X509_free(x509_cert);

  return true;
}

/**
 * Generate a RSA keypair if it doesn't exist already
 * @param public_key_path Path to public part of key
 * @param private_key_path Path to private part of key
 * @return true if the keys are present at the end of this function (either they were created or existed already)
 *         false if key generation failed
 */
bool Crypto::generateRSAKeyPairIfMissing(const std::string &public_key_path, const std::string &private_key_path) {
  if (boost::filesystem::exists(public_key_path) && boost::filesystem::exists(private_key_path)) {
    return true;
  }
  // If one or both are missing, generate them both
  return Crypto::generateRSAKeyPair(public_key_path, private_key_path);
}

bool Crypto::generateRSAKeyPair(const std::string &public_key, const std::string &private_key) {
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
  BIO *bp_public = BIO_new_file(public_key.c_str(), "w");
  ret = PEM_write_bio_PUBKEY(bp_public, pkey);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    BIO_free_all(bp_public);
    return false;
  }

  BIO *bp_private = BIO_new_file(private_key.c_str(), "w");
  ret = PEM_write_bio_RSAPrivateKey(bp_private, r, NULL, NULL, 0, NULL, NULL);
  if (ret != 1) {
    RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
    BN_free(bne);
#endif
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    return false;
  }
  RSA_free(r);
#if AKTUALIZR_OPENSSL_AFTER_11
  BN_free(bne);
#endif
  BIO_free_all(bp_public);
  BIO_free_all(bp_private);
  return true;
}
