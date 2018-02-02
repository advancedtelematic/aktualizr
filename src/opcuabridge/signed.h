#ifndef OPCUABRIDGE_SIGNED_H_
#define OPCUABRIDGE_SIGNED_H_

#include "common.h"

namespace opcuabridge {
class Signed {
 public:
  Signed() {}
  virtual ~Signed() {}
 public:
  const int& get_numberOfTokens() const { return numberOfTokens_; }
  void set_numberOfTokens(const int& numberOfTokens) { numberOfTokens_ = numberOfTokens; }
  const std::vector<int>& get_tokens() const { return tokens_; }
  void set_tokens(const std::vector<int>& tokens) { tokens_ = tokens; }
  const int& get_timestamp() const { return timestamp_; }
  void set_timestamp(const int& timestamp) { timestamp_ = timestamp; }
 protected:
  int numberOfTokens_;
  std::vector<int> tokens_;
  int timestamp_;
 public:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["numberOfTokens"] = get_numberOfTokens();
    v["tokens"] = converto::jsonArray(get_tokens());
    v["timestamp"] = get_timestamp();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_numberOfTokens(v["numberOfTokens"].asUInt());
    set_tokens(converto::stdVector<int>(v["tokens"]));
    set_timestamp(v["timestamp"].asInt());
  }
 private:
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "numberOfTokens_", numberOfTokens_);
    SERIALIZE_FIELD(ar, "tokens_", tokens_);
    SERIALIZE_FIELD(ar, "timestamp_", timestamp_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_SIGNED_H_
