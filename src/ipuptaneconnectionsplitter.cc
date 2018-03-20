#include "ipuptaneconnectionsplitter.h"
#include "uptane/ipuptanesecondary.h"

IpUptaneConnectionSplitter::IpUptaneConnectionSplitter(IpUptaneConnection& conn) : connection(conn) {
  split_thread = std::thread([this]() {
    std::shared_ptr<SecondaryPacket> pack;
    while (connection.in_channel_ >> pack) {
      auto sec = secondaries.find(pack->peer_addr);
      if (sec != secondaries.end()) sec->second->pushMessage(pack);
    }
  });
}

void IpUptaneConnectionSplitter::registerSecondary(Uptane::IpUptaneSecondary& sec) {
  secondaries[sec.getAddr()] = &sec;
}

void IpUptaneConnectionSplitter::send(std::shared_ptr<SecondaryPacket> pack) { connection.out_channel_ << pack; }

bool operator<(const sockaddr_storage& left, const sockaddr_storage right) {
  return memcmp(&left, &right, sizeof(sockaddr_storage)) < 0;
}
