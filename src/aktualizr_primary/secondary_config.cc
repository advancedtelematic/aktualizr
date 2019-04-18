#include <fstream>
#include <iostream>
#include <unordered_map>

#include "logging/logging.h"
#include "secondary_config.h"

namespace Primary {

const char* const IPSecondaryConfig::Type = "ip";

SecondaryConfigParser::Configs SecondaryConfigParser::parse_config_file(const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Specified config file doesn't exist: " + config_file.string());
  }

  auto cfg_file_ext = boost::filesystem::extension(config_file);
  std::shared_ptr<SecondaryConfigParser> cfg_parser;

  if (cfg_file_ext == ".json") {
    cfg_parser = std::make_shared<JsonConfigParser>(config_file);
  } else {  // add your format of configuration file + implement SecondaryConfigParser specialization
    throw std::invalid_argument("Unsupported type of config format: " + cfg_file_ext);
  }

  return cfg_parser->parse();
}

/*
config file example

{
  "ip": [
          {"addr": "127.0.0.1:9031"},
          {"addr": "127.0.0.1:9032"}
  ],
  "socketcan": [
          {"key": "value", "key1": "value1"},
          {"key": "value", "key1": "value1"}
  ]
}

*/

JsonConfigParser::JsonConfigParser(const boost::filesystem::path& config_file) {
  assert(boost::filesystem::exists(config_file));
  std::ifstream json_file_stream(config_file.string());
  Json::Reader json_reader;

  if (!json_reader.parse(json_file_stream, root_, false)) {
    throw std::invalid_argument("Failed to parse secondary config file: " + config_file.string() + ": " +
                                json_reader.getFormattedErrorMessages());
  }
}

SecondaryConfigParser::Configs JsonConfigParser::parse() {
  Configs res_sec_cfg;

  for (Json::ValueIterator it = root_.begin(); it != root_.end(); ++it) {
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

void JsonConfigParser::createIPSecondaryConfig(Configs& configs, Json::Value& ip_sec_cfgs) {
  for (const auto& ip_sec_cfg : ip_sec_cfgs) {
    auto addr = getIPAndPort(ip_sec_cfg[IPSecondaryConfig::AddrField].asString());
    configs.emplace_back(std::make_shared<IPSecondaryConfig>(addr.first, addr.second));
  }
}

}  // namespace Primary
