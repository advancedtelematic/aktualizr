#ifndef AKTUALIZR_BOOTSTRAP_H
#define AKTUALIZR_BOOTSTRAP_H

#include <string>

class Bootstrap {
 public:
  Bootstrap(const std::string& provision_path_in);
  ~Bootstrap();

  std::string getP12Str() const;
  std::string getPkeyPath() const;
  std::string getCertPath() const;
  std::string getCaPath() const;

 private:
  std::string provision_path;
  std::string p12_str;
  std::string pkey_path;
  std::string cert_path;
  std::string ca_path;
};

#endif  // AKTUALIZR_BOOTSTRAP_H
