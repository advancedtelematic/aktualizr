#ifndef OPCUABRIDGE_SIGNATURE_H_
#define OPCUABRIDGE_SIGNATURE_H_

#include "hash.h"

#include "common.h"

namespace opcuabridge {
class Signature {
 public:
  Signature() {}
  virtual ~Signature() {}

  const std::string& getKeyid() const { return keyid_; }
  void setKeyid(const std::string& keyid) { keyid_ = keyid; }
  const SignatureMethod& getMethod() const { return method_; }
  void setMethod(const SignatureMethod& method) { method_ = method; }
  const Hash& getHash() const { return hash_; }
  void setHash(const Hash& hash) { hash_ = hash; }
  const std::string& getValue() const { return value_; }
  void setValue(const std::string& value) { value_ = value; }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["keyid"] = getKeyid();
    v["method"] = static_cast<Json::Value::Int>(getMethod());
    v["hash"] = getHash().wrapMessage();
    v["value"] = getValue();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setKeyid(v["keyid"].asString());
    setMethod(static_cast<SignatureMethod>(v["method"].asInt()));
    Hash h;
    h.unwrapMessage(v["hash"]);
    setHash(h);
    setValue(v["value"].asString());
  }

 protected:
  std::string keyid_;
  SignatureMethod method_;
  Hash hash_;
  std::string value_;

 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "keyid_", keyid_);
    SERIALIZE_FIELD(ar, "method_", method_);
    SERIALIZE_FIELD(ar, "hash_", hash_);
    SERIALIZE_FIELD(ar, "value_", value_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_SIGNATURE_H_
