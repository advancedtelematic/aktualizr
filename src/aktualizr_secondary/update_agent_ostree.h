#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H

#include "update_agent.h"

class OstreeManager;
class KeyManager;

class OstreeUpdateAgent : public UpdateAgent {
 public:
  OstreeUpdateAgent(const boost::filesystem::path& sysroot_path, std::shared_ptr<KeyManager>& key_mngr,
                    std::shared_ptr<OstreeManager>& ostree_pack_man, std::string targetname_prefix)
      : _sysrootPath(sysroot_path),
        _keyMngr(key_mngr),
        _ostreePackMan(ostree_pack_man),
        _targetname_prefix(std::move(targetname_prefix)) {}

 public:
  bool isTargetSupported(const Uptane::Target& target) const override;
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool download(const Uptane::Target& target, const std::string& data) override;
  data::ResultCode::Numeric install(const Uptane::Target& target) override;
  void completeInstall() override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;

 private:
  const boost::filesystem::path& _sysrootPath;
  std::shared_ptr<KeyManager> _keyMngr;
  std::shared_ptr<OstreeManager> _ostreePackMan;
  const ::std::string _targetname_prefix;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_OSTREE_H
