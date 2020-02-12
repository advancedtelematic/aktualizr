#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <unordered_map>
#include <unordered_set>

#include "ipuptanesecondary.h"
#include "secondary.h"
#include "secondary_config.h"

namespace Primary {

using Secondaries = std::vector<std::shared_ptr<Uptane::SecondaryInterface>>;
using SecondaryFactoryRegistry = std::unordered_map<std::string, std::function<Secondaries(const SecondaryConfig&)>>;

static Secondaries createIPSecondaries(const IPSecondariesConfig& config);

static SecondaryFactoryRegistry sec_factory_registry = {
    {IPSecondariesConfig::Type,
     [](const SecondaryConfig& config) {
       auto ip_sec_cgf = dynamic_cast<const IPSecondariesConfig&>(config);
       return createIPSecondaries(ip_sec_cgf);
     }},
    {VirtualSecondaryConfig::Type,
     [](const SecondaryConfig& config) {
       auto virtual_sec_cgf = dynamic_cast<const VirtualSecondaryConfig&>(config);
       return Secondaries({std::make_shared<VirtualSecondary>(virtual_sec_cgf)});
     }},
    //  {
    //     Add another secondary factory here
    //  }
};

static Secondaries createSecondaries(const SecondaryConfig& config) {
  return (sec_factory_registry.at(config.type()))(config);
}

void initSecondaries(Aktualizr& aktualizr, const boost::filesystem::path& config_file) {
  if (!boost::filesystem::exists(config_file)) {
    throw std::invalid_argument("Secondary ECUs config file does not exist: " + config_file.string());
  }

  auto secondary_configs = SecondaryConfigParser::parse_config_file(config_file);

  for (auto& config : secondary_configs) {
    try {
      LOG_INFO << "Creating " << config->type() << " secondaries...";
      Secondaries secondaries = createSecondaries(*config);

      for (const auto& secondary : secondaries) {
        LOG_INFO << "Adding Secondary with ECU serial: " << secondary->getSerial()
                 << " with hardware ID: " << secondary->getHwId();
        aktualizr.AddSecondary(secondary);
      }
    } catch (const std::exception& exc) {
      LOG_ERROR << "Failed to initialize a secondary: " << exc.what();
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
        LOG_ERROR << "Wait for secondaries has failed: " << error_code;
        throw std::runtime_error("Error while waiting for secondary");
      } else {
        LOG_ERROR << "Timeout while waiting for secondary: " << error_code;
        throw std::runtime_error("Timeout while waiting for secondary");
      }
      io_context_.stop();
    });
    accept();
    io_context_.run();
  }

 private:
  void accept() {
    LOG_INFO << "Waiting for connection from " << secondaries_to_wait_for_.size() << " secondaries...";
    acceptor_.async_accept(con_socket_,
                           boost::bind(&SecondaryWaiter::connectionHdlr, this, boost::asio::placeholders::error));
  }

  void connectionHdlr(const boost::system::error_code& error_code) {
    if (!error_code) {
      auto sec_ip = con_socket_.remote_endpoint().address().to_string();
      auto sec_port = con_socket_.remote_endpoint().port();

      LOG_INFO << "Accepted connection from a secondary: (" << sec_ip << ":" << sec_port << ")";
      try {
        auto secondary = Uptane::IpUptaneSecondary::create(sec_ip, sec_port, con_socket_.native_handle());
        if (secondary) {
          connected_secondaries_.push_back(secondary);
        }
      } catch (const std::exception& exc) {
        LOG_ERROR << "Failed to initialize a secondary: " << exc.what();
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
      LOG_ERROR << "Failed to accept connection from a secondary";
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

static Secondaries createIPSecondaries(const IPSecondariesConfig& config) {
  Secondaries result;
  SecondaryWaiter sec_waiter{config.secondaries_wait_port, config.secondaries_timeout_s, result};

  for (auto& ip_sec_cfg : config.secondaries_cfg) {
    auto secondary = Uptane::IpUptaneSecondary::connectAndCreate(ip_sec_cfg.ip, ip_sec_cfg.port);
    if (secondary) {
      result.push_back(secondary);
    } else {
      sec_waiter.addSecondary(ip_sec_cfg.ip, ip_sec_cfg.port);
    }
  }

  sec_waiter.wait();
  return result;
}

}  // namespace Primary
