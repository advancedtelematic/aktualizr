#ifndef OPCUABRIDGE_VERSIONREPORT_H_
#define OPCUABRIDGE_VERSIONREPORT_H_

#include "ecuversionmanifest.h"

#include "common.h"

namespace opcuabridge {
class VersionReport {
 public:
  VersionReport() {}
  virtual ~VersionReport() {}

  const int& getTokenForTimeServer() const { return tokenForTimeServer_; }
  void setTokenForTimeServer(const int& tokenForTimeServer) { tokenForTimeServer_ = tokenForTimeServer; }
  const ECUVersionManifest& getEcuVersionManifest() const { return ecuVersionManifest_; }
  void setEcuVersionManifest(const ECUVersionManifest& ecuVersionManifest) { ecuVersionManifest_ = ecuVersionManifest; }
  INITSERVERNODESET_FUNCTION_DEFINITION(VersionReport)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                      // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                     // ClientWrite(UA_Client*)

  void setOnBeforeReadCallback(MessageOnBeforeReadCallback<VersionReport>::type cb) { on_before_read_cb_ = cb; }
  void setOnAfterWriteCallback(MessageOnAfterWriteCallback<VersionReport>::type cb) { on_after_write_cb_ = cb; }

 protected:
  int tokenForTimeServer_;
  ECUVersionManifest ecuVersionManifest_;

  MessageOnBeforeReadCallback<VersionReport>::type on_before_read_cb_;
  MessageOnAfterWriteCallback<VersionReport>::type on_after_write_cb_;

 private:
  static const char* node_id_;

  Json::Value wrapMessage() const {
    Json::Value v;
    v["tokenForTimeServer"] = getTokenForTimeServer();
    v["ecuVersionManifest"] = getEcuVersionManifest().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    setTokenForTimeServer(v["tokenForTimeServer"].asInt());
    ECUVersionManifest vm;
    vm.unwrapMessage(v["ecuVersionManifest"]);
    setEcuVersionManifest(vm);
  }

  WRAPMESSAGE_FUCTION_DEFINITION(VersionReport)
  UNWRAPMESSAGE_FUCTION_DEFINITION(VersionReport)
  READ_FUNCTION_FRIEND_DECLARATION(VersionReport)
  WRITE_FUNCTION_FRIEND_DECLARATION(VersionReport)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(VersionReport)

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
  SERIALIZE_FUNCTION_FRIEND_DECLARATION

  DEFINE_SERIALIZE_METHOD() {
    SERIALIZE_FIELD(ar, "tokenForTimeServer_", tokenForTimeServer_);
    SERIALIZE_FIELD(ar, "ecuVersionManifest_", ecuVersionManifest_);
  }
#endif  // OPCUABRIDGE_ENABLE_SERIALIZATION
};
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_VERSIONREPORT_H_
