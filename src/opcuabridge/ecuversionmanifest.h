#ifndef OPCUABRIDGE_ECUVERSIONMANIFEST_H_
#define OPCUABRIDGE_ECUVERSIONMANIFEST_H_

#include "ecuversionmanifestsigned.h"
#include "signature.h"

#include "common.h"

namespace opcuabridge {
class ECUVersionManifest {
 public:
  ECUVersionManifest() = default;
  virtual ~ECUVersionManifest() = default;

  const std::vector<Signature>& getSignatures() const { return signatures_; }
  void setSignatures(const std::vector<Signature>& signatures) { signatures_ = signatures; }
  ECUVersionManifestSigned& getEcuVersionManifestSigned() { return ecuVersionManifestSigned_; }
  const ECUVersionManifestSigned& getEcuVersionManifestSigned() const { return ecuVersionManifestSigned_; }
  void setEcuVersionManifestSigned(const ECUVersionManifestSigned& ecuVersionManifestSigned) {
    ecuVersionManifestSigned_ = ecuVersionManifestSigned;
  }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["signatures"] = convert_to::jsonArray(getSignatures());
    v["signed"] = getEcuVersionManifestSigned().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setSignatures(convert_to::stdVector<Signature>(v["signatures"]));
    ECUVersionManifestSigned ms;
    ms.unwrapMessage(v["signed"]);
    setEcuVersionManifestSigned(ms);
  }

 protected:
  std::vector<Signature> signatures_;
  ECUVersionManifestSigned ecuVersionManifestSigned_;

 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "signatures_", signatures_);
    SERIALIZE_FIELD(ar, "ecuVersionManifestSigned_", ecuVersionManifestSigned_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_ECUVERSIONMANIFEST_H_
