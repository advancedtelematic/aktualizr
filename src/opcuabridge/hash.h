#ifndef OPCUABRIDGE_HASH_H_
#define OPCUABRIDGE_HASH_H_

#include "common.h"

namespace opcuabridge {
class Hash {
 public:
  Hash() = default;
  virtual ~Hash() = default;

  const HashFunction& getFunction() const { return function_; }
  void setFunction(const HashFunction& function) { function_ = function; }
  const std::string& getDigest() const { return digest_; }
  void setDigest(const std::string& digest) { digest_ = digest; }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["function"] = getFunction();
    v["digest"] = getDigest();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setFunction(static_cast<HashFunction>(v["function"].asInt()));
    setDigest(v["digest"].asString());
  }

 protected:
  HashFunction function_{};
  std::string digest_;

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
