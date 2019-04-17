#ifndef SECONDARY_CONFIG_H_
#define SECONDARY_CONFIG_H_

#include <json/json.h>
#include <boost/filesystem.hpp>
#include <unordered_map>

namespace Primary {

class SecondaryConfig {
 public:
  SecondaryConfig(const char* type) : type_(type) {}
  virtual const char* type() const { return type_; }
  virtual ~SecondaryConfig() = default;

 private:
  const char* const type_;
};

class IPSecondaryConfig : public SecondaryConfig {
 public:
  static const char* const Type;
  static constexpr const char* const AddrField{"addr"};

  IPSecondaryConfig(std::string addr_ip, uint16_t addr_port)
      : SecondaryConfig(Type), ip(std::move(addr_ip)), port(addr_port) {}

 public:
  const std::string ip;
  const uint16_t port;
};

class SecondaryConfigParser {
 public:
  using Configs = std::vector<std::shared_ptr<SecondaryConfig>>;

  static Configs parse_config_file(const boost::filesystem::path& config_file);
  virtual ~SecondaryConfigParser() = default;

  // TODO implement iterator instead of parse
  virtual Configs parse() = 0;
};

class JsonConfigParser : public SecondaryConfigParser {
 public:
  JsonConfigParser(const boost::filesystem::path& config_file);

  Configs parse() override;

 private:
  static void createIPSecondaryConfig(Configs& configs, Json::Value& ip_sec_cfgs);
  // add here a factory method for another type of secondary config

 private:
  using SecondaryConfigFactoryRegistry = std::unordered_map<std::string, std::function<void(Configs&, Json::Value&)>>;

  SecondaryConfigFactoryRegistry sec_cfg_factory_registry_ = {
      {IPSecondaryConfig::Type, createIPSecondaryConfig}
      // add here factory method for another type of secondary config
  };

  Json::Value root_;
};

}  // namespace Primary

#endif  // SECONDARY_CONFIG_H_
