#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H

#include "update_agent.h"

class FileUpdateAgent : public UpdateAgent {
 public:
  FileUpdateAgent(boost::filesystem::path target_filepath, std::string target_name)
      : target_filepath_{std::move(target_filepath)}, current_target_name_{std::move(target_name)} {}

 public:
  bool isTargetSupported(const Uptane::Target& target) const override;
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;
  bool download(const Uptane::Target& target, const std::string& data) override;
  data::ResultCode::Numeric install(const Uptane::Target& target) override;
  void completeInstall() override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;

 private:
  const boost::filesystem::path target_filepath_;
  std::string current_target_name_;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H
