#include <fstream>
#include <iostream>
#include <unordered_map>

#include <json/json.h>

#include "logging/logging.h"
#include "secondary_config.h"
#include "utilities/utils.h"

namespace Primary {

const char* const IPSecondariesConfig::Type = "IP";

SecondaryConfigParser::Configs SecondaryConfigParser::parse_config_file(const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Specified config file doesn't exist: " + config_file.string());
  }

  auto cfg_file_ext = boost::filesystem::extension(config_file);
  std::unique_ptr<SecondaryConfigParser> cfg_parser;

  if (cfg_file_ext == ".json") {
    cfg_parser = std_::make_unique<JsonConfigParser>(config_file);
  } else {  // add your format of configuration file + implement SecondaryConfigParser specialization
    throw std::invalid_argument("Unsupported type of config format: " + cfg_file_ext);
  }

  return cfg_parser->parse();
}

/*
config file example

{
  "IP": {
                "secondaries_wait_port": 9040,
                "secondaries_wait_timeout": 20,
                "secondaries": [
                        {"addr": "127.0.0.1:9031"}
                        {"addr": "127.0.0.1:9032"}
                ]
  },
  "socketcan": {
                "common-key": "common-value",
                "secondaries": [
                        {"key": "value", "key1": "value1"},
                        {"key": "value", "key1": "value1"}
                ]
  }
}


*/

JsonConfigParser::JsonConfigParser(const boost::filesystem::path& config_file) {
  assert(boost::filesystem::exists(config_file));
  std::ifstream json_file_stream(config_file.string());
  std::string errs;

  if (!Json::parseFromStream(Json::CharReaderBuilder(), json_file_stream, &root_, &errs)) {
    throw std::invalid_argument("Failed to parse secondary config file: " + config_file.string() + ": " + errs);
  }
}

SecondaryConfigParser::Configs JsonConfigParser::parse() {
  Configs res_sec_cfg;

  for (auto it = root_.begin(); it != root_.end(); ++it) {
    std::string secondary_type = it.key().asString();

    if (sec_cfg_factory_registry_.find(secondary_type) == sec_cfg_factory_registry_.end()) {
      LOG_ERROR << "Unsupported type of sescondary config was found: `" << secondary_type
                << "`. Ignoring it and continuing with parsing of other secondary configs";
    } else {
      (sec_cfg_factory_registry_.at(secondary_type))(res_sec_cfg, *it);
    }
  }

  return res_sec_cfg;
}

static std::pair<std::string, uint16_t> getIPAndPort(const std::string& addr) {
  auto del_pos = addr.find_first_of(':');
  if (del_pos == std::string::npos) {
    throw std::invalid_argument("Incorrect address string, couldn't find port delimeter: " + addr);
  }
  std::string ip = addr.substr(0, del_pos);
  uint16_t port = static_cast<uint16_t>(std::stoul(addr.substr(del_pos + 1)));

  return std::make_pair(ip, port);
}

void JsonConfigParser::createIPSecondariesCfg(Configs& configs, const Json::Value& json_ip_sec_cfg) {
  auto resultant_cfg = std::make_shared<IPSecondariesConfig>(
      static_cast<uint16_t>(json_ip_sec_cfg[IPSecondariesConfig::PortField].asUInt()),
      json_ip_sec_cfg[IPSecondariesConfig::TimeoutField].asInt());
  auto secondaries = json_ip_sec_cfg[IPSecondariesConfig::SecondariesField];

  LOG_INFO << "Found IP secondaries config: " << *resultant_cfg;

  for (const auto& secondary : secondaries) {
    auto addr = getIPAndPort(secondary[IPSecondaryConfig::AddrField].asString());
    IPSecondaryConfig sec_cfg{addr.first, addr.second};

    LOG_INFO << "   found IP secondary config: " << sec_cfg;
    resultant_cfg->secondaries_cfg.push_back(sec_cfg);
  }

  configs.push_back(resultant_cfg);
}

void JsonConfigParser::createVirtualSecondariesCfg(Configs& configs, const Json::Value& json_virtual_sec_cfg) {
  for (const auto& json_config : json_virtual_sec_cfg) {
    auto virtual_config = std::make_shared<VirtualSecondaryConfig>(json_config);
    configs.push_back(virtual_config);
  }
}

}  // namespace Primary
