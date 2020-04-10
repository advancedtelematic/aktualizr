#ifndef AKTUALIZR_SECONDARY_FILE_H
#define AKTUALIZR_SECONDARY_FILE_H

#include <memory>

#include "aktualizr_secondary.h"

class FileUpdateAgent;

class AktualizrSecondaryFile : public AktualizrSecondary {
 public:
  static const std::string FileUpdateDefaultFile;

 public:
  AktualizrSecondaryFile(AktualizrSecondaryConfig config);
  AktualizrSecondaryFile(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage,
                         std::shared_ptr<FileUpdateAgent> update_agent = nullptr);

 public:
  data::ResultCode::Numeric receiveData(const uint8_t* data, size_t size);

 protected:
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool isTargetSupported(const Uptane::Target& target) const override;
  data::ResultCode::Numeric installPendingTarget(const Uptane::Target& target) override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;
  void completeInstall() override;

  ReturnCode uploadDataHdlr(Asn1Message& in_msg, Asn1Message& out_msg);

 private:
  std::shared_ptr<FileUpdateAgent> update_agent_;
};

#endif  // AKTUALIZR_SECONDARY_FILE_H
