#ifndef AKTUALIZR_SECONDARY_OSTREE_H
#define AKTUALIZR_SECONDARY_OSTREE_H

#include "aktualizr_secondary.h"

class OstreeUpdateAgent;

class AktualizrSecondaryOstree : public AktualizrSecondary {
 public:
  AktualizrSecondaryOstree(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage);
  AktualizrSecondaryOstree(AktualizrSecondaryConfig config);

 public:
  data::ResultCode::Numeric downloadOstreeUpdate(std::string packed_tls_creds);

 protected:
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool isTargetSupported(const Uptane::Target& target) const override;
  data::ResultCode::Numeric installPendingTarget(const Uptane::Target& target) override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;
  void completeInstall() override;

 private:
  bool hasPendingUpdate() { return storage().hasPendingInstall(); }

  ReturnCode downloadOstreeRev(Asn1Message& in_msg, Asn1Message& out_msg);

 private:
  std::shared_ptr<OstreeUpdateAgent> update_agent_;
};

#endif  // AKTUALIZR_SECONDARY_OSTREE_H
