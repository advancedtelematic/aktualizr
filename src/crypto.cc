#include "crypto.h"

#include <boost/algorithm/hex.hpp>
#include <iostream>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <sodium.h>

#include "logger.h"
#include "utils.h"

std::string Crypto::sha256digest(const std::string &text) {
  EVP_MD_CTX *md_ctx;
  md_ctx = EVP_MD_CTX_create();
  EVP_DigestInit_ex(md_ctx, EVP_sha256(), NULL);
  EVP_DigestUpdate(md_ctx, (const void *)text.c_str(), text.size());
  unsigned char digest[32];
  unsigned int digest_len = 32;
  EVP_DigestFinal_ex(md_ctx, digest, &digest_len);
  EVP_MD_CTX_cleanup(md_ctx);
  EVP_MD_CTX_destroy(md_ctx);
  return std::string((char *)digest, 32);
}

std::string Crypto::RSAPSSSign(const std::string &private_key, const std::string &message) {
  RAND_poll();

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
  unsigned int sign_size = RSA_size(rsa);
  unsigned char EM[sign_size];
  unsigned char pSignature[sign_size];

  std::string digest = Crypto::sha256digest(message);
  int status = RSA_padding_add_PKCS1_PSS(rsa, EM, (const unsigned char *)digest.c_str(), EVP_sha256(),
                                         -1 /* maximum salt length*/);
  if (!status) {
    LOGGER_LOG(LVL_error, "RSA_padding_add_PKCS1_PSS failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return std::string();
  }

  /* perform digital signature */
  status = RSA_private_encrypt(RSA_size(rsa), EM, pSignature, rsa, RSA_NO_PADDING);
  if (status == -1) {
    LOGGER_LOG(LVL_error, "RSA_private_encrypt failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return std::string();
  }
  RSA_free(rsa);
  return std::string((char *)pSignature, sign_size);
}

bool Crypto::RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message) {
  RAND_poll();
  RSA *rsa = NULL;

  BIO *bio = BIO_new_mem_buf(const_cast<char *>(public_key.c_str()), (int)public_key.size());
  if (!PEM_read_bio_RSA_PUBKEY(bio, &rsa, NULL, NULL)) {
    LOGGER_LOG(LVL_error, "PEM_read_bio_RSA_PUBKEY failed with error " << ERR_error_string(ERR_get_error(), NULL));
    return false;
  }
  BIO_free_all(bio);

  unsigned char pDecrypted[RSA_size(rsa)];
  /* now we will verify the signature
     Start by a RAW decrypt of the signature
  */
  int status = RSA_public_decrypt((int)signature.size(), (const unsigned char *)signature.c_str(), pDecrypted, rsa,
                                  RSA_NO_PADDING);
  if (status == -1) {
    LOGGER_LOG(LVL_error, "RSA_public_decrypt failed with error " << ERR_error_string(ERR_get_error(), NULL));
    RSA_free(rsa);
    return false;
  }

  std::string digest = Crypto::sha256digest(message);

  /* verify the data */
  status = RSA_verify_PKCS1_PSS(rsa, (const unsigned char *)digest.c_str(), EVP_sha256(), pDecrypted,
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
  if (public_key.type == "ed25519") {
    return ED25519Verify(boost::algorithm::unhex(public_key.value), boost::algorithm::unhex(signature), message);
  } else if (public_key.type == "rsa") {
    return RSAPSSVerify(public_key.value, Utils::fromBase64(signature), message);
  } else {
    LOGGER_LOG(LVL_error, "unsupported keytype: " << public_key.type);
    return false;
  }
}

bool Crypto::parseP12(FILE *p12_fp, const std::string &p12_password, const std::string &pkey_pem,
                      const std::string &client_pem, const std::string ca_pem) {
  SSLeay_add_all_algorithms();
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
