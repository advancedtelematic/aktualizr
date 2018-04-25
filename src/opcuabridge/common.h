#ifndef OPCUABRIDGE_COMMON_H_
#define OPCUABRIDGE_COMMON_H_

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION
#include "boostarch.h"
#include "utility.h"
#endif

#include <functional>

#include <open62541.h>
#include "json/json.h"

#include <algorithm>
#include <string>
#include <vector>

#include "opcuabridgeconfig.h"

namespace boost {
namespace filesystem {
class path;
}  // namespace filesystem
}  // namespace boost

#define INITSERVERNODESET_FUNCTION_DEFINITION(TYPE)                             \
  void InitServerNodeset(UA_Server *server) {                                   \
    opcuabridge::internal::AddDataSourceVariable<TYPE>(server, node_id_, this); \
  }

#define INITSERVERNODESET_BIN_FUNCTION_DEFINITION(TYPE, BINDATA)                                  \
  void InitServerNodeset(UA_Server *server) {                                                     \
    opcuabridge::internal::AddDataSourceVariable<TYPE>(server, node_id_, this);                   \
    opcuabridge::internal::AddDataSourceVariable<opcuabridge::MessageBinaryData>(                 \
        server, bin_node_id_, BINDATA, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id_))); \
  }

#define INITSERVERNODESET_FILE_FUNCTION_DEFINITION(TYPE)                                       \
  void InitServerNodeset(UA_Server *server) {                                                  \
    opcuabridge::internal::AddDataSourceVariable<TYPE>(server, node_id_, this);                \
    opcuabridge::internal::AddDataSourceVariable<opcuabridge::MessageFileData>(                \
        server, bin_node_id_, this, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id_))); \
  }

#define CLIENTWRITE_FUNCTION_DEFINITION() \
  UA_StatusCode ClientWrite(UA_Client *client) { return opcuabridge::internal::ClientWrite(client, node_id_, this); }

#define CLIENTWRITE_BIN_FUNCTION_DEFINITION(BINDATA)                            \
  UA_StatusCode ClientWrite(UA_Client *client) {                                \
    return opcuabridge::internal::ClientWrite(client, node_id_, this, BINDATA); \
  }

#define CLIENTWRITE_FILE_FUNCTION_DEFINITION()                                         \
  UA_StatusCode ClientWriteFile(UA_Client *client, const boost::filesystem::path &f) { \
    return opcuabridge::internal::ClientWriteFile(client, node_id_, this, f);          \
  }

#define CLIENTREAD_FUNCTION_DEFINITION() \
  UA_StatusCode ClientRead(UA_Client *client) { return opcuabridge::internal::ClientRead(client, node_id_, this); }

#define CLIENTREAD_BIN_FUNCTION_DEFINITION(BINDATA)                            \
  UA_StatusCode ClientRead(UA_Client *client) {                                \
    return opcuabridge::internal::ClientRead(client, node_id_, this, BINDATA); \
  }

#define READ_FUNCTION_FRIEND_DECLARATION(TYPE)                                                                   \
  friend UA_StatusCode read<TYPE>(UA_Server *, const UA_NodeId *, void *, const UA_NodeId *, void *, UA_Boolean, \
                                  const UA_NumericRange *, UA_DataValue *);

#define WRITE_FUNCTION_FRIEND_DECLARATION(TYPE)                                                       \
  friend UA_StatusCode write<TYPE>(UA_Server *, const UA_NodeId *, void *, const UA_NodeId *, void *, \
                                   const UA_NumericRange *, const UA_DataValue *);

#define INTERNAL_FUNCTIONS_FRIEND_DECLARATION(TYPE)                                                                   \
  friend UA_StatusCode opcuabridge::internal::ClientWrite<TYPE>(UA_Client *, const char *, TYPE *);                   \
  friend UA_StatusCode opcuabridge::internal::ClientWrite<TYPE>(UA_Client *, const char *, TYPE *, BinaryDataType *); \
  friend UA_StatusCode opcuabridge::internal::ClientRead<TYPE>(UA_Client *, const char *, TYPE *);                    \
  friend UA_StatusCode opcuabridge::internal::ClientRead<TYPE>(UA_Client *, const char *, TYPE *, BinaryDataType *);  \
  friend UA_StatusCode opcuabridge::internal::ClientWriteFile<TYPE>(UA_Client *, const char *, TYPE *,                \
                                                                    const boost::filesystem::path &);

#define WRAPMESSAGE_FUCTION_DEFINITION(TYPE)  \
  static std::string wrapMessage(TYPE *obj) { \
    Json::Value value = obj->wrapMessage();   \
    Json::FastWriter fw;                      \
    return fw.write(value);                   \
  }

#define UNWRAPMESSAGE_FUCTION_DEFINITION(TYPE)                        \
  static void unwrapMessage(TYPE *obj, const char *msg, size_t len) { \
    Json::Reader rd;                                                  \
    Json::Value value;                                                \
    rd.parse(msg, msg + len, value);                                  \
    obj->unwrapMessage(value);                                        \
  }

#define DEFINE_SERIALIZE_METHOD() \
  template <typename Archive>     \
  inline void serialize(Archive &ar, const unsigned int version)

#define SERIALIZE_FIELD(AR, XML_TAGNAME, FIELD) \
  { utility::make_serialize_field(AR, FIELD)(AR, XML_TAGNAME, FIELD); }

#define SERIALIZE_FUNCTION_FRIEND_DECLARATION friend class boost::serialization::access;

namespace opcuabridge {

extern const UA_UInt16 kNSindex;

extern const char *kLocale;

extern void BoostLogOpcua(UA_LogLevel /*level*/, UA_LogCategory /*category*/, const char * /*msg*/, va_list /*args*/);

enum HashFunction { HASH_FUN_SHA224, HASH_FUN_SHA256, HASH_FUN_SHA384 };

enum SignatureMethod { SIG_METHOD_RSASSA_PSS, SIG_METHOD_ED25519 };

struct MessageBinaryData {};
struct MessageFileData {
  virtual std::string getFullFilePath() const = 0;
};
typedef std::vector<uint8_t> BinaryDataType;

template <typename T>
struct MessageOnBeforeReadCallback {
  typedef std::function<void(T *)> type;
};

template <typename T>
struct MessageOnAfterWriteCallback {
  typedef std::function<void(T *)> type;
};

template <typename T>
UA_StatusCode read(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext, const UA_NodeId *nodeId,
                   void *nodeContext, UA_Boolean sourceTimeStamp, const UA_NumericRange *range,
                   UA_DataValue *dataValue) {
  auto *obj = static_cast<T *>(nodeContext);

  if (obj->on_before_read_cb_) {
    obj->on_before_read_cb_(obj);
  }

  std::string msg = T::wrapMessage(obj);

  UA_Variant_setArrayCopy(&dataValue->value, msg.c_str(), msg.size(), &UA_TYPES[UA_TYPES_BYTE]);
  dataValue->hasValue = true;

  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode read<MessageBinaryData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                      const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                      const UA_NumericRange *range, UA_DataValue *dataValue);

template <>
UA_StatusCode read<MessageFileData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                    const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
                                    const UA_NumericRange *range, UA_DataValue *dataValue);

template <typename T>
UA_StatusCode write(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext, const UA_NodeId *nodeId,
                    void *nodeContext, const UA_NumericRange *range, const UA_DataValue *data) {
  auto *obj = static_cast<T *>(nodeContext);

  if (!UA_Variant_isEmpty(&data->value) && UA_Variant_hasArrayType(&data->value, &UA_TYPES[UA_TYPES_BYTE])) {
    T::unwrapMessage(obj, static_cast<const char *>(data->value.data), data->value.arrayLength);
  }
  if (obj->on_after_write_cb_) {
    obj->on_after_write_cb_(obj);
  }
  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode write<MessageBinaryData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                       const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                       const UA_DataValue *data);

template <>
UA_StatusCode write<MessageFileData>(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
                                     const UA_NodeId *nodeId, void *nodeContext, const UA_NumericRange *range,
                                     const UA_DataValue *data);

namespace internal {

template <typename MessageT>
inline void AddDataSourceVariable(UA_Server *server, const char *node_id, void *node_context,
                                  const UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER)) {
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(const_cast<char *>(kLocale), const_cast<char *>(node_id));
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  UA_DataSource dataSource;
  dataSource.read = &opcuabridge::read<MessageT>;
  dataSource.write = &opcuabridge::write<MessageT>;
  UA_Server_addDataSourceVariableNode(server, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), parent_node_id,
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(kNSindex, const_cast<char *>(node_id)), UA_NODEID_NULL, attr,
                                      dataSource, node_context, nullptr);
}

template <typename MessageT>
inline UA_StatusCode ClientWrite(UA_Client *client, const char *node_id, MessageT *obj) {
  UA_Variant *val = UA_Variant_new();
  std::string msg = MessageT::wrapMessage(obj);
  UA_Variant_setArray(val, const_cast<char *>(msg.c_str()), msg.size(), &UA_TYPES[UA_TYPES_BYTE]);
  val->storageType = UA_VARIANT_DATA_NODELETE;
  UA_StatusCode retval =
      UA_Client_writeValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  UA_Variant_delete(val);
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientWrite(UA_Client *client, const char *node_id, MessageT *obj, BinaryDataType *bin_data) {
  // write binary child node
  UA_Variant *bin_val = UA_Variant_new();
  UA_Variant_setArray(bin_val, &(*bin_data)[0], bin_data->size(), &UA_TYPES[UA_TYPES_BYTE]);
  bin_val->storageType = UA_VARIANT_DATA_NODELETE;
  UA_StatusCode retval =
      UA_Client_writeValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(obj->bin_node_id_)), bin_val);
  UA_Variant_delete(bin_val);

  if (retval == UA_STATUSCODE_GOOD) {
    UA_Variant *val = UA_Variant_new();
    std::string msg = MessageT::wrapMessage(obj);
    UA_Variant_setArray(val, const_cast<char *>(msg.c_str()), msg.size(), &UA_TYPES[UA_TYPES_BYTE]);
    val->storageType = UA_VARIANT_DATA_NODELETE;
    retval = UA_Client_writeValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
    UA_Variant_delete(val);
  }
  return retval;
}

UA_StatusCode ClientWriteFile(UA_Client * /*client*/, const char * /*node_id*/,
                              const boost::filesystem::path & /*file_path*/,
                              std::size_t block_size = OPCUABRIDGE_FILEDATA_WRITE_BLOCK_SIZE);

template <typename MessageT>
inline UA_StatusCode ClientWriteFile(UA_Client *client, const char *node_id, MessageT *obj,
                                     const boost::filesystem::path &file_path) {
  UA_StatusCode retval = ClientWrite<MessageT>(client, node_id, obj);
  if (retval == UA_STATUSCODE_GOOD) {
    retval = ClientWriteFile(client, obj->bin_node_id_, file_path);
  }
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientRead(UA_Client *client, const char *node_id, MessageT *obj) {
  UA_Variant *val = UA_Variant_new();
  UA_StatusCode retval =
      UA_Client_readValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  if (retval == UA_STATUSCODE_GOOD && UA_Variant_hasArrayType(val, &UA_TYPES[UA_TYPES_BYTE])) {
    MessageT::unwrapMessage(obj, static_cast<const char *>(val->data), val->arrayLength);
  }
  UA_Variant_delete(val);
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientRead(UA_Client *client, const char *node_id, MessageT *obj, BinaryDataType *bin_data) {
  UA_Variant *val = UA_Variant_new();
  UA_StatusCode retval =
      UA_Client_readValueAttribute(client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  if (retval == UA_STATUSCODE_GOOD && UA_Variant_hasArrayType(val, &UA_TYPES[UA_TYPES_BYTE])) {
    MessageT::unwrapMessage(obj, static_cast<const char *>(val->data), val->arrayLength);
    // read binary child node
    UA_Variant *bin_val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(
        client, UA_NODEID_STRING(kNSindex, const_cast<char *>(obj->bin_node_id_)), bin_val);
    if (retval == UA_STATUSCODE_GOOD && UA_Variant_hasArrayType(bin_val, &UA_TYPES[UA_TYPES_BYTE])) {
      bin_data->resize(bin_val->arrayLength);
      const auto *src = static_cast<const unsigned char *>(bin_val->data);
      std::copy(src, src + bin_val->arrayLength, bin_data->begin());
    }
    UA_Variant_delete(bin_val);
  }
  UA_Variant_delete(val);
  return retval;
}
}  // namespace internal

namespace convert_to {

template <typename T>
inline Json::Value jsonArray(const std::vector<T> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i) {
    jsonArray[i] = v[i].wrapMessage();
  }
  return jsonArray;
}

template <>
inline Json::Value jsonArray<int>(const std::vector<int> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i) {
    jsonArray[i] = static_cast<Json::Value::Int>(v[i]);
  }
  return jsonArray;
}

template <>
inline Json::Value jsonArray<std::size_t>(const std::vector<std::size_t> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i) {
    jsonArray[i] = static_cast<Json::Value::UInt>(v[i]);
  }
  return jsonArray;
}

template <typename T>
inline std::vector<T> stdVector(const Json::Value &v) {
  std::vector<T> stdv;
  for (int i = 0; i < v.size(); ++i) {
    T item;
    item.unwrapMessage(v[i]);
    stdv.push_back(item);
  }
  return stdv;
}

template <>
inline std::vector<int> stdVector(const Json::Value &v) {
  std::vector<int> stdv;
  stdv.reserve(v.size());
  for (int i = 0; i < v.size(); ++i) {
    stdv.push_back(v[i].asInt());
  }
  return stdv;
}

}  // namespace convert_to

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_COMMON_H_
