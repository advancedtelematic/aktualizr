#ifndef AKTUALIZR_SECONDARY_UPDATE_AGENT_H
#define AKTUALIZR_SECONDARY_UPDATE_AGENT_H

#include "crypto/crypto.h"
#include "uptane/tuf.h"

class UpdateAgent {
 public:
  using Ptr = std::shared_ptr<UpdateAgent>;

 public:
  virtual bool isTargetSupported(const Uptane::Target& target) const = 0;
  virtual bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const = 0;

  virtual bool download(const Uptane::Target& target, const std::string& data) = 0;
  virtual data::ResultCode::Numeric receiveData(const Uptane::Target& target, const uint8_t* data, size_t size) = 0;
  virtual data::ResultCode::Numeric install(const Uptane::Target& target) = 0;
  virtual void completeInstall() = 0;
  virtual data::InstallationResult applyPendingInstall(const Uptane::Target& target) = 0;

  virtual ~UpdateAgent() = default;

 public:
  UpdateAgent(const UpdateAgent&) = delete;
  UpdateAgent& operator=(const UpdateAgent&) = delete;

 protected:
  UpdateAgent() = default;
};

#endif  // AKTUALIZR_SECONDARY_UPDATE_AGENT_H
