#ifndef AKTUALIZR_BOOTSTRAP_H
#define AKTUALIZR_BOOTSTRAP_H

#include <boost/filesystem.hpp>
#include <string>

class Bootstrap {
 public:
  Bootstrap(const boost::filesystem::path& provision_path, const std::string& provision_password);

  static std::string readServerUrl(const boost::filesystem::path& provision_path);
  static std::string readServerCa(const boost::filesystem::path& provision_path);

  std::string getCa() const { return ca; }
  std::string getCert() const { return cert; }
  std::string getPkey() const { return pkey; }

 private:
  std::string ca;
  std::string cert;
  std::string pkey;
};

#endif  // AKTUALIZR_BOOTSTRAP_H
