#ifndef AKTUALIZR_SECONDARY_OSTREE_H
#define AKTUALIZR_SECONDARY_OSTREE_H

#include "aktualizr_secondary.h"

class OstreeUpdateAgent;

class AktualizrSecondaryOstree : public AktualizrSecondary {
 public:
  AktualizrSecondaryOstree(const AktualizrSecondaryConfig& config);
  AktualizrSecondaryOstree(const AktualizrSecondaryConfig& config, const std::shared_ptr<INvStorage>& storage);

  void initialize() override;
  data::ResultCode::Numeric downloadOstreeUpdate(const std::string& packed_tls_creds);

 protected:
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool isTargetSupported(const Uptane::Target& target) const override;
  data::ResultCode::Numeric installPendingTarget(const Uptane::Target& target) override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;
  void completeInstall() override;

 private:
  bool hasPendingUpdate() { return storage().hasPendingInstall(); }

  ReturnCode downloadOstreeRev(Asn1Message& in_msg, Asn1Message& out_msg);

  std::shared_ptr<OstreeUpdateAgent> update_agent_;
};

#endif  // AKTUALIZR_SECONDARY_OSTREE_H
