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
  data::ResultCode::Numeric installResp(const std::string& target_name);

  bool pendingRebootToApplyUpdate() {
    // TODO: it's up to a pack man to know whether there is a pending install or not
    return storage_->hasPendingInstall() && pending_target_.IsValid();
  }

  bool rebootDetected() { return pacman->rebootDetected() && storage_->hasPendingInstall(); }

 private:
  void connectToPrimary();
  bool doFullVerification(const Metadata& metadata);

 private:
  SocketServer socket_server_;
  Uptane::DirectorRepository director_repo_;
  Uptane::ImagesRepository image_repo_;
  Uptane::Target pending_target_{Uptane::Target::Unknown()};
};

// TBD: consider moving this and SotaUptaneClient::secondaryTreehubCredentials() to encapsulate them in one place that
// is shared between IP Secondary's component
void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert, std::string* pkey,
                               std::string* treehub_server);

class Downloader {
 public:
  Downloader() = default;
  virtual ~Downloader() = default;

  Downloader(const Downloader&) = delete;
  Downloader& operator=(const Downloader&) = delete;

  virtual bool download(const Uptane::Target& target, const std::string& data) = 0;
};

class OstreeDirectDownloader : public Downloader {
 public:
  OstreeDirectDownloader(const boost::filesystem::path& sysroot_path, KeyManager& key_mngr)
      : _sysrootPath(sysroot_path), _keyMngr(key_mngr) {}

  bool download(const Uptane::Target& target, const std::string& data) override;

 private:
  static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                        std::string* pkey, std::string* treehub_server);

 private:
  boost::filesystem::path _sysrootPath;
  KeyManager& _keyMngr;
};

class FakeDownloader : public Downloader {
 public:
  bool download(const Uptane::Target& target, const std::string& data) override;
};

#endif  // AKTUALIZR_SECONDARY_H
