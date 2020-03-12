#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "ipuptanesecondary.h"
#include "secondary.h"
#include "secondary_config.h"

namespace Primary {

using Secondaries = std::vector<std::shared_ptr<Uptane::SecondaryInterface>>;
using SecondaryFactoryRegistry =
    std::unordered_map<std::string, std::function<Secondaries(const SecondaryConfig&, Aktualizr& aktualizr)>>;

static Secondaries createIPSecondaries(const IPSecondariesConfig& config, Aktualizr& aktualizr);

static SecondaryFactoryRegistry sec_factory_registry = {
    {IPSecondariesConfig::Type,
     [](const SecondaryConfig& config, Aktualizr& aktualizr) {
       auto ip_sec_cgf = dynamic_cast<const IPSecondariesConfig&>(config);
       return createIPSecondaries(ip_sec_cgf, aktualizr);
     }},
    {VirtualSecondaryConfig::Type,
     [](const SecondaryConfig& config, Aktualizr& /* unused */) {
       auto virtual_sec_cgf = dynamic_cast<const VirtualSecondaryConfig&>(config);
       return Secondaries({std::make_shared<VirtualSecondary>(virtual_sec_cgf)});
     }},
    //  {
    //     Add another secondary factory here
    //  }
};

static Secondaries createSecondaries(const SecondaryConfig& config, Aktualizr& aktualizr) {
  return (sec_factory_registry.at(config.type()))(config, aktualizr);
}

void initSecondaries(Aktualizr& aktualizr, const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Secondary ECUs config file does not exist: " + config_file.string());
  }

  auto secondary_configs = SecondaryConfigParser::parse_config_file(config_file);

  for (auto& config : secondary_configs) {
    try {
      LOG_INFO << "Registering " << config->type() << " Secondaries...";
      Secondaries secondaries = createSecondaries(*config, aktualizr);

      for (const auto& secondary : secondaries) {
        LOG_INFO << "Adding Secondary with ECU serial: " << secondary->getSerial()
                 << " with hardware ID: " << secondary->getHwId();
        aktualizr.AddSecondary(secondary);
      }
    } catch (const std::exception& exc) {
      LOG_ERROR << "Failed to initialize a Secondary: " << exc.what();
      throw exc;
    }
  }
}

class SecondaryWaiter {
 public:
  SecondaryWaiter(uint16_t wait_port, int timeout_s, Secondaries& secondaries)
      : endpoint_{boost::asio::ip::tcp::v4(), wait_port},
        timeout_{static_cast<boost::posix_time::seconds>(timeout_s)},
        timer_{io_context_},
        connected_secondaries_(secondaries) {}

  void addSecondary(const std::string& ip, uint16_t port) { secondaries_to_wait_for_.insert(key(ip, port)); }

  void wait() {
    if (secondaries_to_wait_for_.empty()) {
      return;
    }

    timer_.expires_from_now(timeout_);
    timer_.async_wait([&](const boost::system::error_code& error_code) {
      if (!!error_code) {
        LOG_ERROR << "Wait for Secondaries has failed: " << error_code;
        throw std::runtime_error("Error while waiting for Secondaries");
      } else {
        LOG_ERROR << "Timeout while waiting for Secondaries: " << error_code;
        throw std::runtime_error("Timeout while waiting for Secondaries");
      }
      io_context_.stop();
    });
    accept();
    io_context_.run();
  }

 private:
  void accept() {
    LOG_INFO << "Waiting for connection from " << secondaries_to_wait_for_.size() << " Secondaries...";
    acceptor_.async_accept(con_socket_,
                           boost::bind(&SecondaryWaiter::connectionHdlr, this, boost::asio::placeholders::error));
  }

  void connectionHdlr(const boost::system::error_code& error_code) {
    if (!error_code) {
      auto sec_ip = con_socket_.remote_endpoint().address().to_string();
      auto sec_port = con_socket_.remote_endpoint().port();

      LOG_INFO << "Accepted connection from a Secondary: (" << sec_ip << ":" << sec_port << ")";
      try {
        auto secondary = Uptane::IpUptaneSecondary::create(sec_ip, sec_port, con_socket_.native_handle());
        if (secondary) {
          connected_secondaries_.push_back(secondary);
        }
      } catch (const std::exception& exc) {
        LOG_ERROR << "Failed to initialize a Secondary: " << exc.what();
      }
      con_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
      con_socket_.close();

      secondaries_to_wait_for_.erase(key(sec_ip, sec_port));
      if (!secondaries_to_wait_for_.empty()) {
        accept();
      } else {
        io_context_.stop();
      }
    } else {
      LOG_ERROR << "Failed to accept connection from a Secondary";
    }
  }

  static std::string key(const std::string& ip, uint16_t port) { return (ip + std::to_string(port)); }

 private:
  boost::asio::io_service io_context_;
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::acceptor acceptor_{io_context_, endpoint_};
  boost::asio::ip::tcp::socket con_socket_{io_context_};
  boost::posix_time::seconds timeout_;
  boost::asio::deadline_timer timer_;

  Secondaries& connected_secondaries_;
  std::unordered_set<std::string> secondaries_to_wait_for_;
};

static Secondaries createIPSecondaries(const IPSecondariesConfig& config, Aktualizr& aktualizr) {
  Secondaries result;
  const bool provision = !aktualizr.IsRegistered();

  if (provision) {
    SecondaryWaiter sec_waiter{config.secondaries_wait_port, config.secondaries_timeout_s, result};

    for (const auto& ip_sec_cfg : config.secondaries_cfg) {
      auto secondary = Uptane::IpUptaneSecondary::connectAndCreate(ip_sec_cfg.ip, ip_sec_cfg.port);
      if (secondary) {
        result.push_back(secondary);
      } else {
        sec_waiter.addSecondary(ip_sec_cfg.ip, ip_sec_cfg.port);
      }
    }

    sec_waiter.wait();

    // set ip/port in the db so that we can match everything later
    for (size_t k = 0; k < config.secondaries_cfg.size(); k++) {
      const auto cfg = config.secondaries_cfg[k];
      const auto sec = result[k];
      Json::Value d;
      d["ip"] = cfg.ip;
      d["port"] = cfg.port;
      aktualizr.SetSecondaryData(sec->getSerial(), Utils::jsonToCanonicalStr(d));
    }
  } else {
    auto secondaries_info = aktualizr.GetSecondaries();

    for (const auto& cfg : config.secondaries_cfg) {
      Uptane::SecondaryInterface::Ptr secondary;
      const SecondaryInfo* info = nullptr;

      auto f = std::find_if(secondaries_info.cbegin(), secondaries_info.cend(), [&cfg](const SecondaryInfo& i) {
        Json::Value d = Utils::parseJSON(i.extra);
        return d["ip"] == cfg.ip && d["port"] == cfg.port;
      });

      if (f == secondaries_info.cend() && config.secondaries_cfg.size() == 1 && secondaries_info.size() == 1) {
        // /!\ backward compatibility: handle the case with one secondary, but
        // store the info for later anyway
        info = &secondaries_info[0];
        Json::Value d;
        d["ip"] = cfg.ip;
        d["port"] = cfg.port;
        aktualizr.SetSecondaryData(info->serial, Utils::jsonToCanonicalStr(d));
        LOG_INFO << "Migrated single IP Secondary to new storage format";
      } else if (f == secondaries_info.cend()) {
        // Match the other way if we can
        secondary = Uptane::IpUptaneSecondary::connectAndCreate(cfg.ip, cfg.port);
        if (secondary == nullptr) {
          LOG_ERROR << "Could not instantiate Secondary " << cfg.ip << ":" << cfg.port;
          continue;
        }
        auto f_serial =
            std::find_if(secondaries_info.cbegin(), secondaries_info.cend(),
                         [&secondary](const SecondaryInfo& i) { return i.serial == secondary->getSerial(); });
        if (f_serial == secondaries_info.cend()) {
          LOG_ERROR << "Could not instantiate Secondary " << cfg.ip << ":" << cfg.port;
          continue;
        }
        info = &(*f_serial);
      } else {
        info = &(*f);
      }

      if (secondary == nullptr) {
        secondary =
            Uptane::IpUptaneSecondary::connectAndCheck(cfg.ip, cfg.port, info->serial, info->hw_id, info->pub_key);
      }

      if (secondary != nullptr) {
        result.push_back(secondary);
      } else {
        LOG_ERROR << "Could not instantiate Secondary " << info->serial;
      }
    }
  }

  return result;
}

}  // namespace Primary
