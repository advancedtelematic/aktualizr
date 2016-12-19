#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>

struct CoreConfig {
  CoreConfig()
      : server("http://127.0.0.1:8080"), polling(true), polling_sec(10) {}

  std::string server;
  bool polling;
  unsigned long long polling_sec;
};

struct AuthConfig {
  AuthConfig()
      : server("http://127.0.0.1:9001"),
        client_id("client-id"),
        client_secret("client-secret") {}

  std::string server;
  std::string client_id;
  std::string client_secret;
};

struct DeviceConfig {
  DeviceConfig() : uuid("123e4567-e89b-12d3-a456-426655440000") {}

  std::string uuid;
  // TODO Need to be implemented soon
  // std::string packages_dir;
  // PackageManager package_manager;
  // std::string certificates_path;
  // std::string p12_path;
  // std::string p12_password;
  // std::string system_info;
};

class Config {
 public:
  void updateFromToml(const std::string &filename);

  // config data structures
  CoreConfig core;
  AuthConfig auth;
  DeviceConfig device;
};
#endif  // CONFIG_H_
