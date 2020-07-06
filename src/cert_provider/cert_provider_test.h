#ifndef CERT_PROVIDER_TEST_H_
#define CERT_PROVIDER_TEST_H_

#include <set>
#include <unordered_map>

#include "test_utils.h"

class DeviceCredGenerator : public Process {
 public:
  DeviceCredGenerator(const std::string& exe_path) : Process(exe_path) {}

  class ArgSet {
   private:
    class Param {
     public:
      Param(const std::string& key, ArgSet* arg_set) : key_(key), arg_set_(arg_set) {}
      Param& operator=(const std::string& val) {
        arg_set_->arg_map_[key_] = val;
        return *this;
      }

      void clear() { arg_set_->arg_map_.erase(key_); }

     private:
      const std::string key_;
      ArgSet* arg_set_;
    };

    class Option {
     public:
      Option(const std::string& key, ArgSet* arg_set) : key_(key), arg_set_(arg_set) {}
      void set() { arg_set_->arg_set_.insert(key_); }
      void clear() { arg_set_->arg_set_.erase(key_); }

     private:
      const std::string key_;
      ArgSet* arg_set_;
    };

   public:
    Param fleetCA{"--fleet-ca", this};
    Param fleetCAKey{"--fleet-ca-key", this};
    Param localDir{"--local", this};
    Param directoryPrefix{"--directory", this};
    Param configFile{"--config", this};
    Param validityDays{"--days", this};
    Param countryCode{"--certificate-c", this};
    Param state{"--certificate-st", this};
    Param organization{"--certificate-o", this};
    Param commonName{"--certificate-cn", this};
    Param rsaBits{"--bits", this};
    Param credentialFile{"--credentials", this};

    Option provideRootCA{"--root-ca", this};
    Option provideServerURL{"--server-url", this};

   public:
    operator std::vector<std::string>() const {
      std::vector<std::string> res_vect;

      for (auto val_pair : arg_map_) {
        res_vect.push_back(val_pair.first);
        res_vect.push_back(val_pair.second);
      }

      for (auto key : arg_set_) {
        res_vect.push_back(key);
      }

      return res_vect;
    }

   private:
    std::unordered_map<std::string, std::string> arg_map_;
    std::set<std::string> arg_set_;
  };

  struct OutputPath {
    OutputPath(const std::string& root_dir, const std::string& prefix = "/var/sota/import",
               const std::string& private_key_file = "pkey.pem", const std::string& cert_file = "client.pem")
        : directory{prefix},
          privateKeyFile{private_key_file},
          certFile{cert_file},
          serverRootCA{"root.crt"},
          gtwURLFile{"gateway.url"},
          rootDir{root_dir},
          privateKeyFileFullPath{(rootDir / directory / privateKeyFile)},
          certFileFullPath{rootDir / directory / certFile},
          serverRootCAFullPath{rootDir / directory / serverRootCA},
          gtwURLFileFullPath{rootDir / directory / gtwURLFile} {}

    const std::string directory;
    const std::string privateKeyFile;
    const std::string certFile;
    const std::string serverRootCA;
    const std::string gtwURLFile;

    const boost::filesystem::path rootDir;
    const boost::filesystem::path privateKeyFileFullPath;
    const boost::filesystem::path certFileFullPath;
    const boost::filesystem::path serverRootCAFullPath;
    const boost::filesystem::path gtwURLFileFullPath;
  };
};

#endif  // CERT_PROVIDER_TEST_H_
