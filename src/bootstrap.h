#ifndef AKTUALIZR_BOOTSTRAP_H
#define AKTUALIZR_BOOTSTRAP_H

#include <string>

class Bootstrap {
 public:
  Bootstrap(const std::string& provision_path, const std::string& provision_password);

  std::string getCa() const { return ca; }
  std::string getCert() const { return cert; }
  std::string getPkey() const { return pkey; }

 private:
  std::string p12_str;
  std::string ca;
  std::string cert;
  std::string pkey;
};

#endif  // AKTUALIZR_BOOTSTRAP_H
