#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_common.h"
#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_interface.h"
#include "crypto/keymanager.h"
#include "socket_server.h"
#include "storage/invstorage.h"
#include "utilities/types.h"
#include "utilities/utils.h"

#include "uptane/directorrepository.h"
#include "uptane/imagesrepository.h"
#include "uptane/tuf.h"

#include "aktualizr_secondary_metadata.h"

class AktualizrSecondary : public AktualizrSecondaryInterface, private AktualizrSecondaryCommon {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config, const std::shared_ptr<INvStorage>& storage);
  void run() override;
  void stop() override;

  // implementation of primary's SecondaryInterface
  Uptane::EcuSerial getSerialResp() const;
  Uptane::HardwareIdentifier getHwIdResp() const;
  PublicKey getPublicKeyResp() const;
  Json::Value getManifestResp() const;
  bool putMetadataResp(const Metadata& metadata);
  int32_t getRootVersionResp(bool director) const;
  bool putRootResp(const std::string& root, bool director);
  bool sendFirmwareResp(const std::shared_ptr<std::string>& firmware);

  static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                        std::string* pkey, std::string* treehub_server);

 private:
  void connectToPrimary();
  bool doFullVerification(const Metadata& metadata);

 private:
  SocketServer socket_server_;
  Uptane::DirectorRepository director_repo_;
  Uptane::ImagesRepository image_repo_;
};

#endif  // AKTUALIZR_SECONDARY_H
