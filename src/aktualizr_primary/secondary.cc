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

using Secondaries = std::vector<std::shared_ptr<SecondaryInterface>>;
using SecondaryFactoryRegistry =
    std::unordered_map<std::string, std::function<Secondaries(const SecondaryConfig&, Aktualizr& aktualizr)>>;

static Secondaries createIPSecondaries(const IPSecondariesConfig& config, Aktualizr& aktualizr);

// NOLINTNEXTLINE(cppcoreguidelines-interfaces-global-init)
static SecondaryFactoryRegistry sec_factory_registry = {
    {IPSecondariesConfig::Type,
     [](const SecondaryConfig& config, Aktualizr& aktualizr) {
       auto ip_sec_cgf = dynamic_cast<const IPSecondariesConfig&>(config);
       return createIPSecondaries(ip_sec_cgf, aktualizr);
     }},
    {VirtualSecondaryConfig::Type,
     [](const SecondaryConfig& config, Aktualizr& aktualizr) {
       (void)aktualizr;
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
      LOG_INFO << "Initializing " << config->type() << " Secondaries...";
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
  SecondaryWaiter(Aktualizr& aktualizr, uint16_t wait_port, int timeout_s, Secondaries& secondaries)
      : aktualizr_(aktualizr),
        endpoint_{boost::asio::ip::tcp::v4(), wait_port},
        timeout_{static_cast<boost::posix_time::seconds>(timeout_s)},
        timer_{io_context_},
        connected_secondaries_{secondaries} {}

  void addSecondary(const std::string& ip, uint16_t port) { secondaries_to_wait_for_.insert(key(ip, port)); }

  void wait() {
    if (secondaries_to_wait_for_.empty()) {
      return;
    }

    timer_.expires_from_now(timeout_);
    timer_.async_wait([&](const boost::system::error_code& error_code) {
      if (!!error_code) {
        LOG_ERROR << "Wait for Secondaries has failed: " << error_code;
        throw std::runtime_error("Error while waiting for IP Secondaries");
      } else {
        LOG_ERROR << "Timeout while waiting for Secondaries: " << error_code;
        throw std::runtime_error("Timeout while waiting for IP Secondaries");
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
          // set ip/port in the db so that we can match everything later
          Json::Value d;
          d["ip"] = sec_ip;
          d["port"] = sec_port;
          aktualizr_.SetSecondaryData(secondary->getSerial(), Utils::jsonToCanonicalStr(d));
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

  Aktualizr& aktualizr_;

  boost::asio::io_service io_context_;
  boost::asio::ip::tcp::endpoint endpoint_;
  boost::asio::ip::tcp::acceptor acceptor_{io_context_, endpoint_};
  boost::asio::ip::tcp::socket con_socket_{io_context_};
  boost::posix_time::seconds timeout_;
  boost::asio::deadline_timer timer_;

  Secondaries& connected_secondaries_;
  std::unordered_set<std::string> secondaries_to_wait_for_;
};

// Four options for each Secondary:
// 1. Secondary is configured and stored: nothing to do.
// 2. Secondary is configured but not stored: it must be new. Try to connect to get information and store it. This will
// cause re-registration.
// 3. Same as 2 but cannot connect: abort.
// 4. Secondary is stored but not configured: it must have been removed. Skip it. This will cause re-registration.
static Secondaries createIPSecondaries(const IPSecondariesConfig& config, Aktualizr& aktualizr) {
  Secondaries result;
  Secondaries new_secondaries;
  SecondaryWaiter sec_waiter{aktualizr, config.secondaries_wait_port, config.secondaries_timeout_s, result};
  auto secondaries_info = aktualizr.GetSecondaries();

  for (const auto& cfg : config.secondaries_cfg) {
    SecondaryInterface::Ptr secondary;
    const SecondaryInfo* info = nullptr;

    // Try to match the configured Secondaries to stored Secondaries.
    auto f = std::find_if(secondaries_info.cbegin(), secondaries_info.cend(), [&cfg](const SecondaryInfo& i) {
      Json::Value d = Utils::parseJSON(i.extra);
      return d["ip"] == cfg.ip && d["port"] == cfg.port;
    });

    if (f == secondaries_info.cend() && config.secondaries_cfg.size() == 1 && secondaries_info.size() == 1 &&
        secondaries_info[0].extra.empty()) {
      // /!\ backward compatibility: if we have just one Secondary in the old
      // storage format (before we had the secondary_ecus table) and the
      // configuration, migrate it to the new format.
      info = &secondaries_info[0];
      Json::Value d;
      d["ip"] = cfg.ip;
      d["port"] = cfg.port;
      aktualizr.SetSecondaryData(info->serial, Utils::jsonToCanonicalStr(d));
      LOG_INFO << "Migrated a single IP Secondary to new storage format.";
    } else if (f == secondaries_info.cend()) {
      // Secondary was not found in storage; it must be new.
      secondary = Uptane::IpUptaneSecondary::connectAndCreate(cfg.ip, cfg.port);
      if (secondary == nullptr) {
        LOG_DEBUG << "Could not connect to IP Secondary at " << cfg.ip << ":" << cfg.port
                  << "; now trying to wait for it.";
        sec_waiter.addSecondary(cfg.ip, cfg.port);
      } else {
        result.push_back(secondary);
        // set ip/port in the db so that we can match everything later
        Json::Value d;
        d["ip"] = cfg.ip;
        d["port"] = cfg.port;
        aktualizr.SetSecondaryData(secondary->getSerial(), Utils::jsonToCanonicalStr(d));
      }
      continue;
    } else {
      // The configured Secondary was found in storage.
      info = &(*f);
    }

    if (secondary == nullptr) {
      secondary =
          Uptane::IpUptaneSecondary::connectAndCheck(cfg.ip, cfg.port, info->serial, info->hw_id, info->pub_key);
      if (secondary == nullptr) {
        throw std::runtime_error("Unable to connect to or verify IP Secondary at " + cfg.ip + ":" +
                                 std::to_string(cfg.port));
      }
    }

    result.push_back(secondary);
  }

  sec_waiter.wait();

  return result;
}

}  // namespace Primary
