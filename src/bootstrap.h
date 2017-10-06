#ifndef AKTUALIZR_BOOTSTRAP_H
#define AKTUALIZR_BOOTSTRAP_H

#include <string>

class Bootstrap {
 public:
  Bootstrap(const std::string& provision_path, const std::string& provision_password);

  static std::string readServerUrl(const std::string& provision_path);

  std::string getCa() const { return ca; }
  std::string getCert() const { return cert; }
  std::string getPkey() const { return pkey; }

 private:
  std::string ca;
  std::string cert;
  std::string pkey;
};

#endif  // AKTUALIZR_BOOTSTRAP_H
