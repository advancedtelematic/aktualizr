#ifndef OPCUABRIDGE_SIGNATURE_H_
#define OPCUABRIDGE_SIGNATURE_H_

#include "hash.h"

#include "common.h"

namespace opcuabridge {
class Signature {
 public:
  Signature() {}
  virtual ~Signature() {}
 public:
  const std::string& get_keyid() const { return keyid_; }
  void set_keyid(const std::string& keyid) { keyid_ = keyid; }
  const SignatureMethod& get_method() const { return method_; }
  void set_method(const SignatureMethod& method) { method_ = method; }
  const Hash& get_hash() const { return hash_; }
  void set_hash(const Hash& hash) { hash_ = hash; }
  const std::string& get_value() const { return value_; }
  void set_value(const std::string& value) { value_ = value; }
 protected:
  std::string keyid_;
  SignatureMethod method_;
  Hash hash_;
  std::string value_;
 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["keyid"] = get_keyid();
    v["method"] = static_cast<Json::Value::Int>(get_method());
    v["hash"] = get_hash().wrapMessage();
    v["value"] = get_value();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_keyid(v["keyid"].asString());
    set_method(static_cast<SignatureMethod>(v["method"].asInt()));
    Hash h; h.unwrapMessage(v["hash"]); set_hash(h);
    set_value(v["value"].asString());
  }
 private:
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
