#ifndef IP_UPTANE_CONNECTION_SPLITTER_H_
#define IP_UPTANE_CONNECTION_SPLITTER_H_

#include <map>
#include <thread>
#include "secondary_ipc/ipuptaneconnection.h"

namespace Uptane {
class IpUptaneSecondary;
}  // namespace Uptane

class IpUptaneConnectionSplitter {
 public:
  explicit IpUptaneConnectionSplitter(IpUptaneConnection& conn);
  ~IpUptaneConnectionSplitter() {
    stopped = true;
    split_thread.join();
  }
  void registerSecondary(Uptane::IpUptaneSecondary& sec);
  void send(const std::shared_ptr<SecondaryPacket>& pack);

 private:
  std::map<sockaddr_storage, Uptane::IpUptaneSecondary*> secondaries;
  IpUptaneConnection& connection;

  std::thread split_thread;
  bool stopped{false};
};
#endif  // IP_UPTANE_CONNECTION_SPLITTER_H_
