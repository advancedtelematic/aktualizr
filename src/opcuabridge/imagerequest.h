#ifndef OPCUABRIDGE_IMAGEREQUEST_H_
#define OPCUABRIDGE_IMAGEREQUEST_H_

#include "common.h"

namespace opcuabridge {
class ImageRequest {
 public:
  ImageRequest() {}
  virtual ~ImageRequest() {}
 public:
  const std::string& get_filename() const { return filename_; }
  void set_filename(const std::string& filename) { filename_ = filename; }
  INITSERVERNODESET_FUNCTION_DEFINITION(ImageRequest)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                     // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                    // ClientWrite(UA_Client*)
 protected:
  std::string filename_;
 private:
  static const char* node_id_;
 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["filename"] = get_filename();
    return v;
  }
  void unwrapMessage(Json::Value v) { set_filename(v["filename"].asString()); }
 private:
  WRAPMESSAGE_FUCTION_DEFINITION(ImageRequest)
  UNWRAPMESSAGE_FUCTION_DEFINITION(ImageRequest)
  READ_FUNCTION_FRIEND_DECLARATION(ImageRequest)
  WRITE_FUNCTION_FRIEND_DECLARATION(ImageRequest)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(ImageRequest)
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "filename_", filename_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_IMAGEREQUEST_H_
