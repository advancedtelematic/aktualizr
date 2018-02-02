#ifndef OPCUABRIDGE_ECUVERSIONMANIFESTSIGNED_H_
#define OPCUABRIDGE_ECUVERSIONMANIFESTSIGNED_H_

#include "image.h"

#include "common.h"

namespace opcuabridge {
class ECUVersionManifestSigned {
 public:
  ECUVersionManifestSigned() {}
  virtual ~ECUVersionManifestSigned() {}
 public:
  const std::string& get_ecuIdentifier() const { return ecuIdentifier_; }
  void set_ecuIdentifier(const std::string& ecuIdentifier) { ecuIdentifier_ = ecuIdentifier; }
  const int& get_previousTime() const { return previousTime_; }
  void set_previousTime(const int& previousTime) { previousTime_ = previousTime; }
  const int& get_currentTime() const { return currentTime_; }
  void set_currentTime(const int& currentTime) { currentTime_ = currentTime; }
  const std::string& get_securityAttack() const { return securityAttack_; }
  void set_securityAttack(const std::string& securityAttack) { securityAttack_ = securityAttack; }
  const Image& get_installedImage() const { return installedImage_; }
  void set_installedImage(const Image& installedImage) { installedImage_ = installedImage; }
 protected:
  std::string ecuIdentifier_;
  int previousTime_;
  int currentTime_;
  std::string securityAttack_;
  Image installedImage_;
 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["ecuIdentifier"] = get_ecuIdentifier();
    v["previousTime"] = get_previousTime();
    v["currentTime"] = get_currentTime();
    v["securityAttack"] = get_securityAttack();
    v["installedImage"] = get_installedImage().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_ecuIdentifier(v["ecuIdentifier"].asString());
    set_previousTime(v["previousTime"].asInt());
    set_currentTime(v["currentTime"].asInt());
    set_securityAttack(v["securityAttack"].asString());
    Image i; i.unwrapMessage(v["installedImage"]); set_installedImage(i);
  }
 private:
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
