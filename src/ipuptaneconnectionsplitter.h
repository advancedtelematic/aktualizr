#ifndef IP_UPTANE_CONNECTION_SPLITTER_H_
#define IP_UPTANE_CONNECTION_SPLITTER_H_

#include <map>
#include <thread>
#include "ipuptaneconnection.h"

namespace Uptane {
class IpUptaneSecondary;
}

bool operator<(const sockaddr_storage& left, const sockaddr_storage right);

class IpUptaneConnectionSplitter {
 public:
  IpUptaneConnectionSplitter(IpUptaneConnection& conn);
  void registerSecondary(Uptane::IpUptaneSecondary& sec);
  void send(std::shared_ptr<SecondaryPacket> pack);

 private:
  std::map<sockaddr_storage, Uptane::IpUptaneSecondary*> secondaries;
  IpUptaneConnection& connection;

  std::thread split_thread;
};
#endif  // IP_UPTANE_CONNECTION_SPLITTER_H_
