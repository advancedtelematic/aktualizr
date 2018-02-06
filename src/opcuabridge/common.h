#ifndef OPCUABRIDGE_COMMON_H_
#define OPCUABRIDGE_COMMON_H_

#include "boostarch.h"
#include "utility.h"

#include "json/json.h"
#include <open62541.h>

#include <algorithm>
#include <vector>
#include <string>

#define INITSERVERNODESET_FUNCTION_DEFINITION(TYPE)                            \
  void InitServerNodeset(UA_Server *server) {                                  \
    opcuabridge::internal::AddDataSourceVariable<TYPE>(server, node_id_, this);\
  }

#define INITSERVERNODESET_BIN_FUNCTION_DEFINITION(TYPE, BINDATA)               \
  void InitServerNodeset(UA_Server *server) {                                  \
    opcuabridge::internal::AddDataSourceVariable<TYPE>(server, node_id_, this);\
    opcuabridge::internal::AddDataSourceVariable<opcuabridge::BinaryData>      \
      (server, bin_node_id_, BINDATA,                                          \
       UA_NODEID_STRING(kNSindex, const_cast<char*>(node_id_)));               \
  }

#define CLIENTWRITE_FUNCTION_DEFINITION()                                      \
  UA_StatusCode ClientWrite(UA_Client *client) {                               \
    return opcuabridge::internal::ClientWrite(client, node_id_, this);         \
  }

#define CLIENTWRITE_BIN_FUNCTION_DEFINITION(BINDATA)                           \
  UA_StatusCode ClientWrite(UA_Client *client) {                               \
    return opcuabridge::internal::ClientWrite(client, node_id_, this, BINDATA);\
  }

#define CLIENTREAD_FUNCTION_DEFINITION()                                       \
  UA_StatusCode ClientRead(UA_Client *client) {                                \
    return opcuabridge::internal::ClientRead(client, node_id_, this);          \
  }

#define CLIENTREAD_BIN_FUNCTION_DEFINITION(BINDATA)                            \
  UA_StatusCode ClientRead(UA_Client *client) {                                \
    return opcuabridge::internal::ClientRead(client, node_id_, this, BINDATA); \
  }

#define READ_FUNCTION_FRIEND_DECLARATION(TYPE)                                 \
  friend UA_StatusCode read<TYPE>(UA_Server *, const UA_NodeId *, void *,      \
                                  const UA_NodeId *, void *, UA_Boolean,       \
                                  const UA_NumericRange *, UA_DataValue *);

#define WRITE_FUNCTION_FRIEND_DECLARATION(TYPE)                                \
  friend UA_StatusCode write<TYPE>(                                            \
      UA_Server *, const UA_NodeId *, void *, const UA_NodeId *, void *,       \
      const UA_NumericRange *, const UA_DataValue *);

#define INTERNAL_FUNCTIONS_FRIEND_DECLARATION(TYPE)                            \
  friend UA_StatusCode opcuabridge::internal::ClientWrite<TYPE>(               \
      UA_Client *, const char *, TYPE *);                                      \
  friend UA_StatusCode opcuabridge::internal::ClientWrite<TYPE>(               \
      UA_Client *, const char *, TYPE *, BinaryDataContainer*);                \
  friend UA_StatusCode opcuabridge::internal::ClientRead<TYPE>(                \
      UA_Client *, const char *, TYPE *);                                      \
  friend UA_StatusCode opcuabridge::internal::ClientRead<TYPE>(                \
      UA_Client *, const char *, TYPE *, BinaryDataContainer*);

#define WRAPMESSAGE_FUCTION_DEFINITION(TYPE)                                   \
  static std::string wrapMessage(TYPE *obj) {                                  \
    Json::Value value = obj->wrapMessage();                                    \
    Json::FastWriter fw;                                                       \
    return fw.write(value);                                                    \
  }

#define UNWRAPMESSAGE_FUCTION_DEFINITION(TYPE)                                 \
  static void unwrapMessage(TYPE *obj, const char *msg, size_t len) {          \
    Json::Reader rd;                                                           \
    Json::Value value;                                                         \
    rd.parse(msg, msg + len, value);                                           \
    obj->unwrapMessage(value);                                                 \
  }

#define DEFINE_SERIALIZE_METHOD()                                              \
  template<typename Archive>                                                   \
  inline void serialize(Archive &ar, const unsigned int version)

#define SERIALIZE_FIELD(AR, XML_TAGNAME, FIELD)                                \
  { utility::make_serialize_field(AR, FIELD)(AR, XML_TAGNAME, FIELD); }

#define SERIALIZE_FUNCTION_FRIEND_DECLARATION                                  \
  friend class boost::serialization::access;

namespace opcuabridge {

extern const UA_UInt16 kNSindex;

extern const char *kLocale;

enum HashFunction {
    HASH_FUN_SHA224,
    HASH_FUN_SHA256,
    HASH_FUN_SHA384
};

enum SignatureMethod {
    SIG_METHOD_RSASSA_PSS,
    SIG_METHOD_ED25519
};

struct BinaryData {};
typedef std::vector<unsigned char> BinaryDataContainer;

template <typename T>
UA_StatusCode read(UA_Server *server, const UA_NodeId *sessionId,
                   void *sessionContext, const UA_NodeId *nodeId,
                   void *nodeContext, UA_Boolean sourceTimeStamp,
                   const UA_NumericRange *range, UA_DataValue *dataValue) {

  std::string msg = T::wrapMessage(static_cast<T*>(nodeContext));

  UA_Variant_setArrayCopy(&dataValue->value, msg.c_str(), msg.size(),
                          &UA_TYPES[UA_TYPES_BYTE]);
  dataValue->hasValue = true;

  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode read<BinaryData>(UA_Server *server, const UA_NodeId *sessionId,
                               void *sessionContext, const UA_NodeId *nodeId,
                               void *nodeContext, UA_Boolean sourceTimeStamp,
                               const UA_NumericRange *range, UA_DataValue *dataValue);

template <typename T>
UA_StatusCode write(UA_Server *server, const UA_NodeId *sessionId,
                    void *sessionContext, const UA_NodeId *nodeId,
                    void *nodeContext, const UA_NumericRange *range,
                    const UA_DataValue *data) {

  if (!UA_Variant_isEmpty(&data->value) &&
      UA_Variant_hasArrayType(&data->value, &UA_TYPES[UA_TYPES_BYTE])) {
    T::unwrapMessage(static_cast<T*>(nodeContext),
                     static_cast<const char*>(data->value.data),
                     data->value.arrayLength);
  }
  return UA_STATUSCODE_GOOD;
}

template <>
UA_StatusCode write<BinaryData>(UA_Server *server, const UA_NodeId *sessionId,
                                void *sessionContext, const UA_NodeId *nodeId,
                                void *nodeContext, const UA_NumericRange *range,
                                const UA_DataValue *data);

namespace internal {

template <typename MessageT>
inline void AddDataSourceVariable(UA_Server *server,
                                  const char *node_id,
                                  void *node_context,
                                  const UA_NodeId parent_node_id =
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER)) {
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT(const_cast<char *>(kLocale),
                                      const_cast<char *>(node_id));
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  UA_DataSource dataSource;
  dataSource.read = &opcuabridge::read<MessageT>;
  dataSource.write = &opcuabridge::write<MessageT>;
  UA_Server_addDataSourceVariableNode(
      server, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)),
      parent_node_id,
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
      UA_QUALIFIEDNAME(kNSindex, const_cast<char *>(node_id)), UA_NODEID_NULL, attr,
      dataSource, node_context, NULL);
}

template <typename MessageT>
inline UA_StatusCode ClientWrite(UA_Client *client, const char *node_id,
                                 MessageT *obj) {
  UA_Variant *val = UA_Variant_new();
  std::string msg = MessageT::wrapMessage(obj);
  UA_Variant_setArray(val, const_cast<char *>(msg.c_str()), msg.size(),
                      &UA_TYPES[UA_TYPES_BYTE]);
  val->storageType = UA_VARIANT_DATA_NODELETE;
  UA_StatusCode retval = UA_Client_writeValueAttribute(
      client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  UA_Variant_delete(val);
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientWrite(UA_Client *client, const char *node_id,
                                 MessageT *obj, BinaryDataContainer* bin_data) {
  UA_Variant *val = UA_Variant_new();
  std::string msg = MessageT::wrapMessage(obj);
  UA_Variant_setArray(val, const_cast<char *>(msg.c_str()), msg.size(),
                      &UA_TYPES[UA_TYPES_BYTE]);
  val->storageType = UA_VARIANT_DATA_NODELETE;
  UA_StatusCode retval = UA_Client_writeValueAttribute(
      client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  if (retval == UA_STATUSCODE_GOOD) {
      // write binary child node
      UA_Variant *bin_val = UA_Variant_new();
      UA_Variant_setArray(bin_val, &(*bin_data)[0], bin_data->size(),
                          &UA_TYPES[UA_TYPES_BYTE]);
      bin_val->storageType = UA_VARIANT_DATA_NODELETE;
      retval = UA_Client_writeValueAttribute(
          client, UA_NODEID_STRING(kNSindex, const_cast<char *>(obj->bin_node_id_)), bin_val);
      UA_Variant_delete(bin_val);
  }
  UA_Variant_delete(val);
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientRead(UA_Client *client, const char *node_id,
                                MessageT *obj) {
  UA_Variant *val = UA_Variant_new();
  UA_StatusCode retval = UA_Client_readValueAttribute(
      client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  if (retval == UA_STATUSCODE_GOOD &&
      UA_Variant_hasArrayType(val, &UA_TYPES[UA_TYPES_BYTE])) {
    MessageT::unwrapMessage(obj, static_cast<const char*>(val->data),
                            val->arrayLength);
  }
  UA_Variant_delete(val);
  return retval;
}

template <typename MessageT>
inline UA_StatusCode ClientRead(UA_Client *client, const char *node_id,
                                MessageT *obj, BinaryDataContainer* bin_data) {
  UA_Variant *val = UA_Variant_new();
  UA_StatusCode retval = UA_Client_readValueAttribute(
      client, UA_NODEID_STRING(kNSindex, const_cast<char *>(node_id)), val);
  if (retval == UA_STATUSCODE_GOOD &&
      UA_Variant_hasArrayType(val, &UA_TYPES[UA_TYPES_BYTE])) {
    MessageT::unwrapMessage(obj, static_cast<const char*>(val->data),
                            val->arrayLength);
    // read binary child node
    UA_Variant *bin_val = UA_Variant_new();
    UA_StatusCode retval = UA_Client_readValueAttribute(
        client, UA_NODEID_STRING(kNSindex, const_cast<char *>(obj->bin_node_id_)), bin_val);
    if (retval == UA_STATUSCODE_GOOD &&
        UA_Variant_hasArrayType(bin_val, &UA_TYPES[UA_TYPES_BYTE])) {
      bin_data->resize(bin_val->arrayLength);
      const unsigned char* src = 
        static_cast<const unsigned char*>(bin_val->data);
      std::copy(src, src + bin_val->arrayLength, bin_data->begin());
    }
    UA_Variant_delete(bin_val);
  }
  UA_Variant_delete(val);
  return retval;
}
} // namespace internal

namespace convert_to {

template <typename T> inline Json::Value jsonArray(const std::vector<T> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i)
    jsonArray[i] = v[i].wrapMessage();
  return jsonArray;
}

template <> inline Json::Value jsonArray<int>(const std::vector<int> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i)
    jsonArray[i] = static_cast<Json::Value::Int>(v[i]);
  return jsonArray;
}

template <>
inline Json::Value jsonArray<std::size_t>(const std::vector<std::size_t> &v) {
  Json::Value jsonArray;
  jsonArray.resize(v.size());
  for (int i = 0; i < v.size(); ++i)
    jsonArray[i] = static_cast<Json::Value::UInt>(v[i]);
  return jsonArray;
}

template <typename T> inline std::vector<T> stdVector(const Json::Value &v) {
  std::vector<T> stdv;
  for (int i = 0; i < v.size(); ++i) {
    T item;
    item.unwrapMessage(v[i]);
    stdv.push_back(item);
  }
  return stdv;
}

template <> inline std::vector<int> stdVector(const Json::Value &v) {
  std::vector<int> stdv;
  for (int i = 0; i < v.size(); ++i)
    stdv.push_back(v[i].asInt());
  return stdv;
}

} // namespace convert_to

} // namespace opcuabridge

#endif // OPCUABRIDGE_COMMON_H_
