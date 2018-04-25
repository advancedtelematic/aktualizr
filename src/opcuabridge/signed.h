#ifndef OPCUABRIDGE_SIGNED_H_
#define OPCUABRIDGE_SIGNED_H_

#include "common.h"

namespace opcuabridge {
class Signed {
 public:
  Signed() = default;
  virtual ~Signed() = default;

  const std::vector<int>& getTokens() const { return tokens_; }
  void setTokens(const std::vector<int>& tokens) { tokens_ = tokens; }
  const int& getTimestamp() const { return timestamp_; }
  void setTimestamp(const int& timestamp) { timestamp_ = timestamp; }

  Json::Value wrapMessage() const {
    Json::Value v;
    v["tokens"] = convert_to::jsonArray(getTokens());
    v["timestamp"] = getTimestamp();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setTokens(convert_to::stdVector<int>(v["tokens"]));
    setTimestamp(v["timestamp"].asInt());
  }

 protected:
  std::vector<int> tokens_;
  int timestamp_{};

 private:
#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "tokens_", tokens_);
    SERIALIZE_FIELD(ar, "timestamp_", timestamp_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_SIGNED_H_
