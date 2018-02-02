#ifndef OPCUABRIDGE_VERSIONREPORT_H_
#define OPCUABRIDGE_VERSIONREPORT_H_

#include "ecuversionmanifest.h"

#include "common.h"

namespace opcuabridge {
class VersionReport {
 public:
  VersionReport() {}
  virtual ~VersionReport() {}

 public:
  const int& get_tokenForTimeServer() const { return tokenForTimeServer_; }
  void set_tokenForTimeServer(const int& tokenForTimeServer) { tokenForTimeServer_ = tokenForTimeServer; }
  const ECUVersionManifest& get_ecuVersionManifest() const { return ecuVersionManifest_; }
  void set_ecuVersionManifest(const ECUVersionManifest& ecuVersionManifest) {
    ecuVersionManifest_ = ecuVersionManifest;
  }
  INITSERVERNODESET_FUNCTION_DEFINITION(VersionReport)  // InitServerNodeset(UA_Server*)
  CLIENTREAD_FUNCTION_DEFINITION()                      // ClientRead(UA_Client*)
  CLIENTWRITE_FUNCTION_DEFINITION()                     // ClientWrite(UA_Client*)
 protected:
  int tokenForTimeServer_;
  ECUVersionManifest ecuVersionManifest_;

 private:
  static const char* node_id_;

 private:
  Json::Value wrapMessage() const {
    Json::Value v;
    v["tokenForTimeServer"] = get_tokenForTimeServer();
    v["ecuVersionManifest"] = get_ecuVersionManifest().wrapMessage();
    return v;
  }
  void unwrapMessage(Json::Value v) {
    set_tokenForTimeServer(v["tokenForTimeServer"].asInt());
    ECUVersionManifest vm;
    vm.unwrapMessage(v["ecuVersionManifest"]);
    set_ecuVersionManifest(vm);
  }

 private:
  WRAPMESSAGE_FUCTION_DEFINITION(VersionReport)
  UNWRAPMESSAGE_FUCTION_DEFINITION(VersionReport)
  READ_FUNCTION_FRIEND_DECLARATION(VersionReport)
  WRITE_FUNCTION_FRIEND_DECLARATION(VersionReport)
  INTERNAL_FUNCTIONS_FRIEND_DECLARATION(VersionReport)
 private:
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
