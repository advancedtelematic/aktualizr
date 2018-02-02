#ifndef OPCUABRIDGE_ECUVERSIONMANIFEST_H_
#define OPCUABRIDGE_ECUVERSIONMANIFEST_H_

#include "ecuversionmanifestsigned.h"
#include "signature.h"

#include "common.h"

namespace opcuabridge {
class ECUVersionManifest {
 public:
  ECUVersionManifest() {}
  virtual ~ECUVersionManifest() {}

 public:
  const std::size_t& get_numberOfSignatures() const { return numberOfSignatures_; }
  void set_numberOfSignatures(const std::size_t& numberOfSignatures) { numberOfSignatures_ = numberOfSignatures; }
  const std::vector<Signature>& get_signatures() const { return signatures_; }
  void set_signatures(const std::vector<Signature>& signatures) { signatures_ = signatures; }
  const ECUVersionManifestSigned& get_ecuVersionManifestSigned() const { return ecuVersionManifestSigned_; }
  void set_ecuVersionManifestSigned(const ECUVersionManifestSigned& ecuVersionManifestSigned) {
    ecuVersionManifestSigned_ = ecuVersionManifestSigned;
  }

 protected:
  std::size_t numberOfSignatures_;
  std::vector<Signature> signatures_;
  ECUVersionManifestSigned ecuVersionManifestSigned_;

 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["numberOfSignatures"] = static_cast<Json::Value::UInt>(get_numberOfSignatures());
    v["signatures"] = converto::jsonArray(get_signatures());
    v["ecuVersionManifestSigned"] = get_ecuVersionManifestSigned().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_numberOfSignatures(v["numberOfSignatures"].asUInt());
    set_signatures(converto::stdVector<Signature>(v["signatures"]));
    ECUVersionManifestSigned ms;
    ms.unwrapMessage(v["ecuVersionManifestSigned"]);
    set_ecuVersionManifestSigned(ms);
  }

 private:
 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "numberOfSignatures_", numberOfSignatures_);
    SERIALIZE_FIELD(ar, "signatures_", signatures_);
    SERIALIZE_FIELD(ar, "ecuVersionManifestSigned_", ecuVersionManifestSigned_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_ECUVERSIONMANIFEST_H_
