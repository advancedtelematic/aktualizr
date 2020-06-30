#ifndef UPTANE_TEST_COMMON_H_
#define UPTANE_TEST_COMMON_H_

#include <string>
#include <vector>

#include "json/json.h"
#include "libaktualizr/config.h"
#include "libaktualizr/aktualizr.h"

#include "utilities/utils.h"
#include "uptane/tuf.h"
#include "virtualsecondary.h"
#include "primary/sotauptaneclient.h"

static const char* sec_public_key = "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyjUeAzozBEccaGFAJ2Q3\n9QBfItH5i5O7yLRjZlKcEnWnFsxAWHUn5W/msRgZN/pXUrlax0wvrvMvHHLwZA2J\nz+UQApzSqj53HPVAcCH6kB9x0r9PM/0vVTKtmcrdSHj7jJ2yAW2T4Vo/eKlpvz3w\n9kTPAj0j1f5LvUgX5VIjUnsQK1LGzMwnleHk2dkWeWnt3OqomnO7V5C0jkDi58tG\nJ6fnyCYWcMUbpMaldXVXqmQ+iBkWxBjZ99+XJSRjdsskC7x8u8t+sA146VDB977r\nN8D+i+P1tAe810crciUqpYNenDYx47aAm6gaDWr7oeDzp3HyCjx4dZi9Z85rVE36\n8wIDAQAB\n-----END PUBLIC KEY-----\n";
static const char* sec_private_key = "-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQEAyjUeAzozBEccaGFAJ2Q39QBfItH5i5O7yLRjZlKcEnWnFsxA\nWHUn5W/msRgZN/pXUrlax0wvrvMvHHLwZA2Jz+UQApzSqj53HPVAcCH6kB9x0r9P\nM/0vVTKtmcrdSHj7jJ2yAW2T4Vo/eKlpvz3w9kTPAj0j1f5LvUgX5VIjUnsQK1LG\nzMwnleHk2dkWeWnt3OqomnO7V5C0jkDi58tGJ6fnyCYWcMUbpMaldXVXqmQ+iBkW\nxBjZ99+XJSRjdsskC7x8u8t+sA146VDB977rN8D+i+P1tAe810crciUqpYNenDYx\n47aAm6gaDWr7oeDzp3HyCjx4dZi9Z85rVE368wIDAQABAoIBAA0WlxS6Zab3O11+\nPfrOv9h5566HTNG+BD+ffXeYDUYcm24cVmXjX2u4bIQ1/RvkdlaCbN/NjKCUWQ5M\nWkb/oVX1i62/nNssI+WZ8kvPxzog7usnOucwkim/mAEGYoBYZF/brTPudc32W3lh\n7dhVGA24snWAo5ssVJax3eoYAPVLqFK5Pb8VUxpHtjERMDDUxM3w6WGXLxuBdA5s\n5vIdv+XrdiQhdPn1HMYEBBInkkYK8w4UytOCAS1/3xfVi2QwX5H9bHkduFpjLSQt\n2StWR9Kh4I80xXp7FwGpfkdUn+3qj5WwneuGY/JnD7AzjDlAThj0AE9iaYjkzXKJ\nVD4ULmECgYEA+UGQ1aglftFuTO427Xmi7tHhooo9U1pKMrg5CkCLkA+MudFzMEgj\npRtDdj8lTTWHEIYQXo5hhZfhk63j89RAKRz1MDFOvgknE8yJa9rSyCAEcwzRzXcY\n3WtWozEZ+5u4KYFHhGjJCSqVFdwyXmjP9ldb35Uxh06OuTbdNkSbiUsCgYEAz62t\nJ1EftTkd/YA/9Poq1deil5g0btPXnMJMj7C99dexNAXuVhS10Rz1Hi74wCFEbkcV\nGL/8U80pER9YYYeFUmqs1pYu7zwcYBT+iNrvFaPifid8FqlJEJ727swnWdpzXpwv\n/6q0h3JXU2odrEMNaGqiPycHQ/45EWMbCtpSs/kCgYEAwjMgWicA17bqvkuXRhzQ\nIkwqBU65ixi82JmJ73/sfNhwp1IV8hcylnAQdq+qK2a6Ddi2JkW+m6yDF2GTSiUj\nvCSQr/SqygsthBKHOx4pvbycWtsxF2lkWRdJUCpweQWRTd0o0HQntdmUgIyoPcBh\nzyevMBr4lNhTAOFLJv37RNMCgYAQq+ODjXqbJKuopvv7YX3Azt+phbln0C+10M8u\nlcSaEKeUAongdScnU0jGFIU5fzIsHB6wbvEFlSmfy0FgCu4D8LZRP5si71Njzyuj\ntteMiCxtbiQC+bH42JoAD3l1OBkc1jLwNjbpzJ7//jvFkVhpMm413Z8ysRzJrYgF\nNgN/mQKBgQDNT2nFoqanlQPkZekqNQNcVMHPyOWP40z4HC5JD1Z5F18Kg3El4EdS\nNfwaFGRT5qiFJBmmzl+6EFmUrrBNtV01zQ6rO+xgy2Y7qUQMNAUMjh1cCpWwUlN0\ng4aT/RawS5WpWN3+lEs4Ouxpgg4ZStXNZRJkSDHwZpkXtFfKzsEXaA==\n-----END RSA PRIVATE KEY-----\n";

struct UptaneTestCommon {

  class TestAktualizr: public Aktualizr {
   public:
    TestAktualizr(Config& config): Aktualizr(config) {}

    TestAktualizr(Config& config,
                  std::shared_ptr<INvStorage> storage,
                  std::shared_ptr<HttpInterface> http)
      : Aktualizr(config, storage, http) {

      if (boost::filesystem::exists(config.uptane.secondary_config_file)) {
        for (const auto& item : Primary::VirtualSecondaryConfig::create_from_file(config.uptane.secondary_config_file)) {
          AddSecondary(std::make_shared<Primary::VirtualSecondary>(item));
        }
      }
    }

    std::shared_ptr<SotaUptaneClient>& uptane_client() { return uptane_client_; }
  };

  class TestUptaneClient: public SotaUptaneClient
  {
   public:
    TestUptaneClient(Config &config_in,
                     std::shared_ptr<INvStorage> storage_in,
                     std::shared_ptr<HttpInterface> http_client,
                     std::shared_ptr<event::Channel> events_channel_in):
      SotaUptaneClient(config_in, storage_in, http_client, events_channel_in) {

      if (boost::filesystem::exists(config_in.uptane.secondary_config_file)) {
          for (const auto& item : Primary::VirtualSecondaryConfig::create_from_file(config_in.uptane.secondary_config_file)) {
            addSecondary(std::make_shared<Primary::VirtualSecondary>(item));
          }
      }
    }

    TestUptaneClient(Config &config_in,
                     std::shared_ptr<INvStorage> storage_in,
                     std::shared_ptr<HttpInterface> http_client) : TestUptaneClient(config_in, storage_in, http_client, nullptr) {}

    TestUptaneClient(Config &config_in,
                     std::shared_ptr<INvStorage> storage_in) : TestUptaneClient(config_in, storage_in, std::make_shared<HttpClient>()) {}
  };

  static Primary::VirtualSecondaryConfig addDefaultSecondary(Config& config, const TemporaryDirectory& temp_dir,
                                                             const std::string& serial, const std::string& hw_id,
                                                             bool hardcoded_keys = true) {
    Primary::VirtualSecondaryConfig ecu_config;

    ecu_config.partial_verifying = false;
    ecu_config.full_client_dir = temp_dir.Path();
    ecu_config.ecu_serial = serial;
    ecu_config.ecu_hardware_id = hw_id;
    ecu_config.ecu_private_key = "sec.priv";
    ecu_config.ecu_public_key = "sec.pub";
    ecu_config.firmware_path = temp_dir / "firmware.txt";
    ecu_config.target_name_path = temp_dir / "firmware_name.txt";
    ecu_config.metadata_path = temp_dir / "secondary_metadata";

    config.uptane.secondary_config_file = temp_dir / boost::filesystem::unique_path() / "virtual_secondary_conf.json";
    ecu_config.dump(config.uptane.secondary_config_file);

    if (hardcoded_keys) {
      // store hard-coded keys to make the tests run WAY faster
      Utils::writeFile((ecu_config.full_client_dir / ecu_config.ecu_private_key), std::string(sec_private_key));
      Utils::writeFile((ecu_config.full_client_dir / ecu_config.ecu_public_key), std::string(sec_public_key));
    }
    return ecu_config;
  }

  static Primary::VirtualSecondaryConfig altVirtualConfiguration(const boost::filesystem::path& client_dir) {
    Primary::VirtualSecondaryConfig ecu_config;

    ecu_config.partial_verifying = false;
    ecu_config.full_client_dir = client_dir;
    ecu_config.ecu_serial = "ecuserial3";
    ecu_config.ecu_hardware_id = "hw_id3";
    ecu_config.ecu_private_key = "sec.priv";
    ecu_config.ecu_public_key = "sec.pub";
    ecu_config.firmware_path = client_dir / "firmware.txt";
    ecu_config.target_name_path = client_dir / "firmware_name.txt";
    ecu_config.metadata_path = client_dir / "secondary_metadata";

    return ecu_config;
  }

  static Config makeTestConfig(const TemporaryDirectory& temp_dir, const std::string& url) {
    Config conf("tests/config/basic.toml");
    conf.uptane.director_server = url + "/director";
    conf.uptane.repo_server = url + "/repo";
    conf.provision.server = url;
    conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
    conf.provision.primary_ecu_hardware_id = "primary_hw";
    conf.storage.path = temp_dir.Path();
    conf.import.base_path = temp_dir.Path() / "import";
    conf.pacman.images_path = temp_dir.Path() / "images";
    conf.tls.server = url;
    conf.bootloader.reboot_sentinel_dir = temp_dir.Path();
    UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");
    return conf;
  }

  static std::vector<Uptane::Target> makePackage(const std::string &serial, const std::string &hw_id) {
    std::vector<Uptane::Target> packages_to_install;
    Json::Value ot_json;
    ot_json["custom"]["ecuIdentifiers"][serial]["hardwareId"] = hw_id;
    ot_json["custom"]["targetFormat"] = "OSTREE";
    ot_json["length"] = 0;
    ot_json["hashes"]["sha256"] = serial;
    packages_to_install.emplace_back(serial, ot_json);
    return packages_to_install;
  }
};

#endif // UPTANE_TEST_COMMON_H_


