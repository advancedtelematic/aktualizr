#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include "secondary_ipc/aktualizr_secondary_ipc.h"
#include "secondary_ipc/ipuptaneconnectionsplitter.h"
#include "uptane/secondaryinterface.h"

namespace Uptane {
class IpUptaneSecondary : public SecondaryInterface {
 public:
  explicit IpUptaneSecondary(const SecondaryConfig& conf) : SecondaryInterface(conf), connection(nullptr) {}

  // SecondaryInterface implementation
  PublicKey getPublicKey() override;
  bool putMetadata(const MetaPack& meta_pack) override;
  int32_t getRootVersion(bool director) override;
  bool putRoot(Uptane::Root root, bool director) override;
  bool sendFirmware(const std::string& data) override;
  Json::Value getManifest() override;

  // extra methods
  void pushMessage(const std::shared_ptr<SecondaryPacket>& pack) { pending_messages << pack; }
  sockaddr_storage getAddr() { return sconfig.ip_addr; }
  void connect(IpUptaneConnectionSplitter* conn) { connection = conn; }

 private:
  bool sendRecv(std::unique_ptr<SecondaryMessage> mes, std::shared_ptr<SecondaryPacket>& resp,
                std::chrono::milliseconds to = std::chrono::milliseconds(1000));
  SecondaryPacket::ChanType pending_messages;
  IpUptaneConnectionSplitter* connection;
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
