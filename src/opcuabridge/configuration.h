#ifndef OPCUABRIDGE_CONFIGURATION_H_
#define OPCUABRIDGE_CONFIGURATION_H_

#include "common.h"

#include "utilities/types.h"

#include <utility>

namespace opcuabridge {
class Configuration {
 public:
  Configuration() = default;
  virtual ~Configuration() = default;

  const std::string& getSerial() const { return serial_; }
  void setSerial(const std::string& serial) { serial_ = serial; }
  const std::string& getHwId() const { return hwid_; }
  void setHwId(const std::string& hwid) { hwid_ = hwid; }
  const KeyType& getPublicKeyType() const { return public_key_type_; }
  void setPublicKeyType(const KeyType& public_key_type) { public_key_type_ = public_key_type; }
  const std::string& getPublicKey() const { return public_key_; }
  void setPublicKey(const std::string& public_key) { public_key_ = public_key; }

  INITSERVERNODESET_FUNCTION_DEFINITION(Configuration)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                      // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                     // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<Configuration>::type cb) {
    on_before_read_cb_ = std::move(cb);
  }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<Configuration>::type cb) {
    on_after_write_cb_ = std::move(cb);
  }

 protected:
  KeyType public_key_type_{};
  std::string hwid_;
  std::string public_key_;
  std::string serial_;

  MessageOnBeforeReadCallback<Configuration>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<Configuration>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["hwid"] = getHwId();
    v["public_key_type"] = getPublicKeyType();
    v["public_key"] = getPublicKey();
    v["serial"] = getSerial();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setHwId(v["hwid"].asString());
    setPublicKeyType(static_cast<KeyType>(v["public_key_type"].asInt()));
    setPublicKey(v["public_key"].asString());
    setSerial(v["serial"].asString());
  }

  WRAPMESSAGE_FUCTION_DEFINITION(Configuration)
  UNWRAPMESSAGE_FUCTION_DEFINITION(Configuration)
  READ_FUNCTION_FRIEND_DECLARATION(Configuration)
  WRITE_FUNCTION_FRIEND_DECLARATION(Configuration)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(Configuration)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "hwid_", hwid_);
    SERIALIZE_FIELD(ar, "serial_", serial_);
    SERIALIZE_FIELD(ar, "public_key_", public_key_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_CONFIGURATION_H_
