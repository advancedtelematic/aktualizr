#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H

#include "update_agent.h"

class OstreeManager;
class KeyManager;

class OstreeUpdateAgent : public UpdateAgent {
 public:
  OstreeUpdateAgent(const boost::filesystem::path& sysroot_path, std::shared_ptr<KeyManager>& key_mngr,
                    std::shared_ptr<OstreeManager>& ostree_pack_man, std::string targetname_prefix)
      : sysrootPath_(sysroot_path),
        keyMngr_(key_mngr),
        ostreePackMan_(ostree_pack_man),
        targetname_prefix_(std::move(targetname_prefix)) {}

 public:
  bool isTargetSupported(const Uptane::Target& target) const override;
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool download(const Uptane::Target& target, const std::string& data) override;
  data::ResultCode::Numeric install(const Uptane::Target& target) override;
  void completeInstall() override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;

 private:
  const boost::filesystem::path& sysrootPath_;
  std::shared_ptr<KeyManager> keyMngr_;
  std::shared_ptr<OstreeManager> ostreePackMan_;
  const ::std::string targetname_prefix_;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H
