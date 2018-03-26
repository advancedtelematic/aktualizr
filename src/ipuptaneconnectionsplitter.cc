#include "ipuptaneconnectionsplitter.h"
#include "uptane/ipuptanesecondary.h"

IpUptaneConnectionSplitter::IpUptaneConnectionSplitter(IpUptaneConnection& conn) : connection(conn) {
  split_thread = std::thread([this]() {
    std::shared_ptr<SecondaryPacket> pack;
    while (!stopped && connection.in_channel_ >> pack) {
      auto sec = secondaries.find(pack->peer_addr);
      if (sec != secondaries.end()) {
        sec->second->pushMessage(pack);
      }
    }
  });
}

void IpUptaneConnectionSplitter::registerSecondary(Uptane::IpUptaneSecondary& sec) {
  secondaries[sec.getAddr()] = &sec;
  sec.connect(this);
}

void IpUptaneConnectionSplitter::send(std::shared_ptr<SecondaryPacket> pack) { connection.out_channel_ << pack; }

bool operator<(const sockaddr_storage& left, const sockaddr_storage& right) {
  if (left.ss_family == AF_INET) {
    throw std::runtime_error("IPv4 addresses are not supported");
  } else {
    const unsigned char* left_addr = reinterpret_cast<const sockaddr_in6*>(&left)->sin6_addr.s6_addr;
    const unsigned char* right_addr = reinterpret_cast<const sockaddr_in6*>(&right)->sin6_addr.s6_addr;
    int res = memcmp(left_addr, right_addr, 16);

    return (res < 0);
  }
}
