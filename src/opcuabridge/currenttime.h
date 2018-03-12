#ifndef OPCUABRIDGE_CURRENTTIME_H_
#define OPCUABRIDGE_CURRENTTIME_H_

#include "signature.h"
#include "signed.h"

#include "common.h"

namespace opcuabridge {
class CurrentTime {
 public:
  CurrentTime() {}
  virtual ~CurrentTime() {}

  const std::vector<Signature>& getSignatures() const { return signatures_; }
  void setSignatures(const std::vector<Signature>& signatures) { signatures_ = signatures; }
  const Signed& getSigned() const { return signed_; }
  void setSigned(const Signed& s) { signed_ = s; }

  INITSERVERNODESET_FUNCTION_DEFINITION(CurrentTime)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                    // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                   // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<CurrentTime>::type cb) { on_before_read_cb_ = cb; }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<CurrentTime>::type cb) { on_after_write_cb_ = cb; }

 protected:
  std::vector<Signature> signatures_;
  Signed signed_;

  MessageOnBeforeReadCallback<CurrentTime>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<CurrentTime>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["signatures"] = convert_to::jsonArray(getSignatures());
    v["signed"] = getSigned().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setSignatures(convert_to::stdVector<Signature>(v["signatures"]));
    Signed s;
    s.unwrapMessage(v["signed"]);
    setSigned(s);
  }

  WRAPMESSAGE_FUCTION_DEFINITION(CurrentTime)
  UNWRAPMESSAGE_FUCTION_DEFINITION(CurrentTime)
  READ_FUNCTION_FRIEND_DECLARATION(CurrentTime)
  WRITE_FUNCTION_FRIEND_DECLARATION(CurrentTime)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(CurrentTime)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "signatures_", signatures_);
    SERIALIZE_FIELD(ar, "signed_", signed_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_CURRENTTIME_H_
