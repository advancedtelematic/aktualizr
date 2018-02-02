#ifndef OPCUABRIDGE_HASH_H_
#define OPCUABRIDGE_HASH_H_

#include "common.h"

namespace opcuabridge {
class Hash {
 public:
  Hash() {}
  virtual ~Hash() {}
 public:
  const HashFunction& get_function() const { return function_; }
  void set_function(const HashFunction& function) { function_ = function; }
  const std::string& get_digest() const { return digest_; }
  void set_digest(const std::string& digest) { digest_ = digest; }
 protected:
  HashFunction function_;
  std::string digest_;
 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["function"] = get_function();
    v["digest"] = get_digest();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_function(static_cast<HashFunction>(v["function"].asInt()));
    set_digest(v["digest"].asString());
  }
 private:
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "function_", function_);
    SERIALIZE_FIELD(ar, "digest_", digest_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_HASH_H_
