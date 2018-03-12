#ifndef OPCUABRIDGE_ECUVERSIONMANIFESTSIGNED_H_
#define OPCUABRIDGE_ECUVERSIONMANIFESTSIGNED_H_

#include "image.h"

#include "common.h"

namespace opcuabridge {
class ECUVersionManifestSigned {
 public:
  ECUVersionManifestSigned() {}
  virtual ~ECUVersionManifestSigned() {}

  const std::string& getEcuIdentifier() const { return ecuIdentifier_; }
  void setEcuIdentifier(const std::string& ecuIdentifier) { ecuIdentifier_ = ecuIdentifier; }
  const int& getPreviousTime() const { return previousTime_; }
  void setPreviousTime(const int& previousTime) { previousTime_ = previousTime; }
  const int& getCurrentTime() const { return currentTime_; }
  void setCurrentTime(const int& currentTime) { currentTime_ = currentTime; }
  const std::string& getSecurityAttack() const { return securityAttack_; }
  void setSecurityAttack(const std::string& securityAttack) { securityAttack_ = securityAttack; }
  const Image& getInstalledImage() const { return installedImage_; }
  void setInstalledImage(const Image& installedImage) { installedImage_ = installedImage; }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["ecuIdentifier"] = getEcuIdentifier();
    v["previousTime"] = getPreviousTime();
    v["currentTime"] = getCurrentTime();
    v["securityAttack"] = getSecurityAttack();
    v["installedImage"] = getInstalledImage().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setEcuIdentifier(v["ecuIdentifier"].asString());
    setPreviousTime(v["previousTime"].asInt());
    setCurrentTime(v["currentTime"].asInt());
    setSecurityAttack(v["securityAttack"].asString());
    Image i;
    i.unwrapMessage(v["installedImage"]);
    setInstalledImage(i);
  }

 protected:
  std::string ecuIdentifier_;
  int previousTime_;
  int currentTime_;
  std::string securityAttack_;
  Image installedImage_;

 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "ecuIdentifier_", ecuIdentifier_);
    SERIALIZE_FIELD(ar, "previousTime_", previousTime_);
    SERIALIZE_FIELD(ar, "currentTime_", currentTime_);
    SERIALIZE_FIELD(ar, "securityAttack_", securityAttack_);
    SERIALIZE_FIELD(ar, "installedImage_", installedImage_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_ECUVERSIONMANIFESTSIGNED_H_
