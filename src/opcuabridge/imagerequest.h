#ifndef OPCUABRIDGE_IMAGEREQUEST_H_
#define OPCUABRIDGE_IMAGEREQUEST_H_

#include "common.h"

namespace opcuabridge {
class ImageRequest {
 public:
  ImageRequest() {}
  virtual ~ImageRequest() {}

  const std::string& getFilename() const { return filename_; }
  void setFilename(const std::string& filename) { filename_ = filename; }
  INITSERVERNODESET_FUNCTION_DEFINITION(ImageRequest)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                     // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                    // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<ImageRequest>::type cb) { on_before_read_cb_ = cb; }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<ImageRequest>::type cb) { on_after_write_cb_ = cb; }

 protected:
  std::string filename_;

  MessageOnBeforeReadCallback<ImageRequest>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<ImageRequest>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = getFilename();
    return v;
  }
  void unwrapMessage(Json::Value v) { setFilename(v["filename"].asString()); }

  WRAPMESSAGE_FUCTION_DEFINITION(ImageRequest)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageRequest)
  READ_FUNCTION_FRIEND_DECLARATION(ImageRequest)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageRequest)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageRequest)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() { SERIALIZE_FIELD(ar, "filename_", filename_); }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGEREQUEST_H_
