#ifndef CRYPTO_P11_CONFIG_H_
#define CRYPTO_P11_CONFIG_H_

#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <libaktualizr/config_utils.h>

// declare p11 types as incomplete so that the header can be used without libp11
struct PKCS11_ctx_st;
struct PKCS11_slot_st;

struct P11Config {
  boost::filesystem::path module;
  std::string pass;
  std::string uptane_key_id;
  std::string tls_cacert_id;
  std::string tls_pkey_id;
  std::string tls_clientcert_id;

  void updateFromPropertyTree(const boost::property_tree::ptree &pt) {
    CopyFromConfig(module, "module", pt);
    CopyFromConfig(pass, "pass", pt);
    CopyFromConfig(uptane_key_id, "uptane_key_id", pt);
    CopyFromConfig(tls_cacert_id, "tls_cacert_id", pt);
    CopyFromConfig(tls_pkey_id, "tls_pkey_id", pt);
    CopyFromConfig(tls_clientcert_id, "tls_clientcert_id", pt);
  }

  void writeToStream(std::ostream &out_stream) const {
    writeOption(out_stream, module, "module");
    writeOption(out_stream, pass, "pass");
    writeOption(out_stream, uptane_key_id, "uptane_key_id");
    writeOption(out_stream, tls_cacert_id, "tls_ca_id");
    writeOption(out_stream, tls_pkey_id, "tls_pkey_id");
    writeOption(out_stream, tls_clientcert_id, "tls_clientcert_id");
  }
};

#endif  // CRYPTO_P11_CONFIG_H_
