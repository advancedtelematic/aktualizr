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
 public:
  const std::size_t& get_numberOfSignatures() const { return numberOfSignatures_; }
  void set_numberOfSignatures(const std::size_t& numberOfSignatures) { numberOfSignatures_ = numberOfSignatures; }
  const std::vector<Signature>& get_signatures() const { return signatures_; }
  void set_signatures(const std::vector<Signature>& signatures) { signatures_ = signatures; }
  const Signed& get_signed() const { return signed_; }
  void set_signed(const Signed& s) { signed_ = s; }
  INITSERVERNODESET_FUNCTION_DEFINITION(CurrentTime)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                    // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                   // ClientWrite(UA_Client*)
 protected:
  std::size_t numberOfSignatures_;
  std::vector<Signature> signatures_;
  Signed signed_;
 private:
  static const char* node_id_;
 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["numberOfSignatures"] = static_cast<Json::Value::UInt>(get_numberOfSignatures());
    v["signatures"] = converto::jsonArray(get_signatures());
    v["signed"] = get_signed().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_numberOfSignatures(v["numberOfSignatures"].asUInt());
    set_signatures(converto::stdVector<Signature>(v["signatures"]));
    Signed s; s.unwrapMessage(v["signed"]); set_signed(s);
  }
 private:
  WRAPMESSAGE_FUCTION_DEFINITION(CurrentTime)
  UNWRAPMESSAGE_FUCTION_DEFINITION(CurrentTime)
  READ_FUNCTION_FRIEND_DECLARATION(CurrentTime)
  WRITE_FUNCTION_FRIEND_DECLARATION(CurrentTime)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(CurrentTime)
 private:
  #ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "numberOfSignatures_", numberOfSignatures_);
    SERIALIZE_FIELD(ar, "signatures_", signatures_);
    SERIALIZE_FIELD(ar, "signed_", signed_);
  }
  #endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_CURRENTTIME_H_
