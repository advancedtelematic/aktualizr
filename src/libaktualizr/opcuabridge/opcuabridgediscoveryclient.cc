#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>

#include <boost/asio.hpp>
#include <boost/phoenix/core/argument.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/scope_exit.hpp>

#include "logging/logging.h"

#include "opcuabridgediscoveryclient.h"

using boost::phoenix::arg_names::arg1;
using boost::phoenix::arg_names::arg2;

namespace ba = boost::asio;
namespace phoenix = boost::phoenix;

namespace opcuabridge {
namespace discovery {

Client::endpoints_list_type Client::getDiscoveredEndPoints() {
  endpoints_list_type discovered_endpoints_list;
  struct ifaddrs *ifaddr = nullptr, *ifa;

  BOOST_SCOPE_EXIT(&ifaddr) {  // NOLINT
    if (ifaddr != nullptr) {
      freeifaddrs(ifaddr);
    }
  }
  BOOST_SCOPE_EXIT_END

  if (getifaddrs(&ifaddr) == -1) {
    LOG_ERROR << "OPC-UA discovery client: unable to get available network interfaces";
    return discovered_endpoints_list;
  }
  std::size_t i = 0;
  char addr[NI_MAXHOST];
  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next, ++i) {
    if (ifa->ifa_addr != nullptr && ifa->ifa_addr->sa_family == AF_INET6 &&
        0 == getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), addr, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) &&
        !ba::ip::address_v6::from_string(addr).is_loopback()) {
      collectDiscoveredEndPointsOnIface(if_nametoindex(ifa->ifa_name));
    }
  }
  for (const auto& ep : discovered_endpoints_) {
    discovered_endpoints_list.push_back(discovered_endpoint{ep.first, ep.second});
  }
  return discovered_endpoints_list;
}

void Client::collectDiscoveredEndPointsOnIface(unsigned int iface) {
  try {
    ba::io_service io_service;

    ba::ip::udp::endpoint ep(ba::ip::address_v6::from_string(OPCUA_DISCOVERY_SERVICE_MULTICAST_ADDR), port_);

    ba::ip::udp::socket s(io_service);
    s.open(ba::ip::udp::v6());

    s.set_option(ba::ip::multicast::outbound_interface(iface));

    boost::system::error_code error_code;

    bool discovery_timeout_expired = false;
    ba::deadline_timer timer(io_service);
    timer.expires_from_now(boost::posix_time::seconds(OPCUA_DISCOVERY_SERVICE_TIMEOUT_SEC));

    timer.async_wait((phoenix::ref(error_code) = arg1, phoenix::ref(discovery_timeout_expired) = true));

    std::size_t bytes_transferred;
    int request_attempts_count = OPCUA_DISCOVERY_SERVICE_ATTEMPTS;
    discovery_service_data_type request = OPCUA_DISCOVERY_SERVICE_REQUEST, response;
    while (!discovery_timeout_expired) {
      if (--request_attempts_count >= 0) {
        s.send_to(ba::buffer(request), ep);
      }

      ba::ip::udp::endpoint discovered_ep;
      error_code = ba::error::would_block;
      s.async_receive_from(ba::buffer(response), discovered_ep,
                           (phoenix::ref(error_code) = arg1, phoenix::ref(bytes_transferred) = arg2));
      do {
        io_service.run_one();
      } while (!discovery_timeout_expired && error_code == ba::error::would_block);
      std::string discovered_ep_address = discovered_ep.address().to_string();
      if (error_code != nullptr) {
        throw boost::system::system_error(error_code);
      }
      if (!discovery_timeout_expired && discovered_endpoints_.count(discovered_ep_address) == 0) {
        EndPointServiceType type =
            (response == no_lds_response_ ? kNoLDS : response == lds_response_ ? kLDS : kUnknown);
        discovered_endpoints_.insert(std::make_pair(discovered_ep_address, type));
      }
    }
  } catch (std::exception& ex) {
    LOG_ERROR << "OPC-UA discovery client: " << ex.what();
  }
}

}  // namespace discovery
}  // namespace opcuabridge
