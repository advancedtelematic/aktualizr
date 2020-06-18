#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H

#include "update_agent.h"

class FileUpdateAgent : public UpdateAgent {
 public:
  FileUpdateAgent(boost::filesystem::path target_filepath, std::string target_name)
      : target_filepath_{std::move(target_filepath)},
        new_target_filepath_{target_filepath_.string() + ".newtarget"},
        current_target_name_{std::move(target_name)} {}

 public:
  bool isTargetSupported(const Uptane::Target& target) const override;
  bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const override;

  virtual data::InstallationResult receiveData(const Uptane::Target& target, const uint8_t* data, size_t size);
  data::InstallationResult install(const Uptane::Target& target) override;

  void completeInstall() override;
  data::InstallationResult applyPendingInstall(const Uptane::Target& target) override;

 private:
  static Hash getTargetHash(const Uptane::Target& target);

 private:
  const boost::filesystem::path target_filepath_;
  const boost::filesystem::path new_target_filepath_;
  std::string current_target_name_;
  std::shared_ptr<MultiPartHasher> new_target_hasher_;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_FILE_H
