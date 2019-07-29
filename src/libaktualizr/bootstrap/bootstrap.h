#ifndef AKTUALIZR_BOOTSTRAP_H
#define AKTUALIZR_BOOTSTRAP_H

#include <boost/filesystem.hpp>
#include <string>

class Bootstrap {
 public:
  Bootstrap(const boost::filesystem::path& provision_path, const std::string& provision_password);

  static void readTlsP12(const std::string& p12_str, const std::string& provision_password, std::string& pkey,
                         std::string& cert, std::string& ca);
  static std::string readServerUrl(const boost::filesystem::path& provision_path);
  static std::string readServerCa(const boost::filesystem::path& provision_path);

  std::string getCa() const { return ca_; }
  std::string getCert() const { return cert_; }
  std::string getPkey() const { return pkey_; }

 private:
  std::string ca_;
  std::string cert_;
  std::string pkey_;
};

#endif  // AKTUALIZR_BOOTSTRAP_H
