#include "secondary_ipc/ipuptaneconnectionsplitter.h"
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
