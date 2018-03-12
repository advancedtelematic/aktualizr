#ifndef OPCUABRIDGE_UTILITY_H_
#define OPCUABRIDGE_UTILITY_H_

namespace opcuabridge {
namespace utility {

template <typename A, typename T>
struct serialize_field {};

#define SERIALIZE_FIELD_DECL_BEGIN(STREAM) \
  template <typename T>                    \
  struct serialize_field<STREAM, T> {      \
  void operator()(STREAM &ar, const std::string &xml_tag, T &d)

#define SERIALIZE_FIELD_DECL_END \
  }                              \
  ;

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_OUT_XML_STREAM) {
  ar &boost::serialization::make_nvp(xml_tag.c_str(), d);
}
SERIALIZE_FIELD_DECL_END

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_IN_XML_STREAM) { ar &boost::serialization::make_nvp(xml_tag.c_str(), d); }
SERIALIZE_FIELD_DECL_END

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_OUT_BIN_STREAM) { ar &d; }
SERIALIZE_FIELD_DECL_END

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_IN_BIN_STREAM) { ar &d; }
SERIALIZE_FIELD_DECL_END

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_OUT_TEXT_STREAM) { ar &d; }
SERIALIZE_FIELD_DECL_END

SERIALIZE_FIELD_DECL_BEGIN(DATA_SERIALIZATION_IN_TEXT_STREAM) { ar &d; }
SERIALIZE_FIELD_DECL_END

#undef SERIALIZE_FIELD_DECL_BEGIN
#undef SERIALIZE_FIELD_DECL_END

template <typename A, typename T>
inline serialize_field<A, T> make_serialize_field(A &ar, T &d) {
  return serialize_field<A, T>();
}

}  // namespace utility
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_UTILITY_H_
