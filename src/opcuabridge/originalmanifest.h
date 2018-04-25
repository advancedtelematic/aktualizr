#ifndef OPCUABRIDGE_ORIGINALMANIFEST_H_
#define OPCUABRIDGE_ORIGINALMANIFEST_H_

#include <utility>

#include "common.h"

namespace opcuabridge {
class OriginalManifest {
 public:
  typedef BinaryDataType block_type;

  OriginalManifest() = default;
  virtual ~OriginalManifest() = default;

  block_type& getBlock() { return block_; }
  const block_type& getBlock() const { return block_; }
  void setBlock(const block_type& block) { block_ = block; }
  INITSERVERNODESET_BIN_FUNCTION_DEFINITION(OriginalManifest, &block_)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_BIN_FUNCTION_DEFINITION(&block_)                           // ClientRead(UA_Client*)
  CLIENTWRITE_BIN_FUNCTION_DEFINITION(&block_)                          // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<OriginalManifest>::type cb) {
    on_before_read_cb_ = std::move(cb);
  }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<OriginalManifest>::type cb) {
    on_after_write_cb_ = std::move(cb);
  }

 protected:
  block_type block_;

  MessageOnBeforeReadCallback<OriginalManifest>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<OriginalManifest>::type on_after_write_cb_;

 private:
  static const char* node_id_;
  static const char* bin_node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    return v;
  }
  void unwrapMessage(const Json::Value& v) {}

  WRAPMESSAGE_FUCTION_DEFINITION(OriginalManifest)
  UNWRAPMESSAGE_FUCTION_DEFINITION(OriginalManifest)
  READ_FUNCTION_FRIEND_DECLARATION(OriginalManifest)
  WRITE_FUNCTION_FRIEND_DECLARATION(OriginalManifest)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(OriginalManifest)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() { SERIALIZE_FIELD(ar, "block_", block_); }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_ORIGINALMANIFEST_H_
