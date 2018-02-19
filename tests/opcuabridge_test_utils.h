#ifndef OPCUABRIDGE_TEST_H_
#define OPCUABRIDGE_TEST_H_

#include "test_utils.h"
#include "opcuabridge/opcuabridge.h"

#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>
#include <boost/preprocessor/cat.hpp>

#define OPCUABRIDGE_TEST_MESSAGES_DEFINITION    \
    BOOST_PP_TUPLE_TO_LIST(7,                   \
    (                                           \
        (VersionReport  , vr),                  \
        (CurrentTime    , ct),                  \
        (MetadataFiles  , mds),                 \
        (MetadataFile   , md),                  \
        (ImageRequest   , ir),                  \
        (ImageFile      , img_file),            \
        (ImageBlock     , img_block)            \
    ))

#define OPCUABRIDGE_TEST_MESSAGES_TYPE(T)       BOOST_PP_TUPLE_ELEM(2, 0, T)
#define OPCUABRIDGE_TEST_MESSAGES_ID(T)         BOOST_PP_TUPLE_ELEM(2, 1, T)

#define DEFINE_MESSAGE(r, unused, e) \
    opcuabridge::OPCUABRIDGE_TEST_MESSAGES_TYPE(e) OPCUABRIDGE_TEST_MESSAGES_ID(e);

#define INIT_SERVER_NODESET(r, SERVER, e) \
    OPCUABRIDGE_TEST_MESSAGES_ID(e).InitServerNodeset(SERVER);

#define LOAD_TRANSFER_CHECK(r, CLIENT, e) \
    LoadTransferCheck<opcuabridge::OPCUABRIDGE_TEST_MESSAGES_TYPE(e)>   \
        (CLIENT, BOOST_PP_STRINGIZE(OPCUABRIDGE_TEST_MESSAGES_TYPE(e)));

template<typename T> inline std::string GetMessageFileName();

#define GET_MESSAGE_FILENAME(r, unused, e)                                                                      \
    template<> inline std::string GetMessageFileName<opcuabridge::OPCUABRIDGE_TEST_MESSAGES_TYPE(e)>() {        \
        return std::string("opcuabridge_") + BOOST_PP_STRINGIZE(OPCUABRIDGE_TEST_MESSAGES_TYPE(e)) + ".xml"; }

BOOST_PP_LIST_FOR_EACH(GET_MESSAGE_FILENAME, _, OPCUABRIDGE_TEST_MESSAGES_DEFINITION)

namespace opcuabridge_test_utils {

opcuabridge::Signature CreateSignature(const std::string& keyid, const opcuabridge::SignatureMethod& method,
                                       const std::string& hash, const std::string& value);

opcuabridge::Signed CreateSigned(std::size_t n);

template <typename MessageT>
std::string GetMessageDumpFilePath(const TemporaryDirectory& temp_dir) {
  return (temp_dir / GetMessageFileName<MessageT>()).native();
}

template <typename MessageT>
std::string GetMessageResponseFilePath(const TemporaryDirectory& temp_dir) {
  return (temp_dir / (GetMessageFileName<MessageT>() + ".response")).native();
}

#ifdef OPCUABRIDGE_ENABLE_SERIALIZATION

template <typename MessageT>
bool SerializeMessage(const TemporaryDirectory& temp_dir,
                      const std::string& roottag, const MessageT& m) {
  std::string filename = GetMessageDumpFilePath<MessageT>(temp_dir);
  std::ofstream file(filename.c_str());

  try {
    boost::archive::xml_oarchive oa(file);
    oa << boost::serialization::make_nvp(roottag.c_str(), m);
    file.flush();
  } catch (...) {
    return false;
  }
  return true;
}

#endif

void BoostLogServer(UA_LogLevel level, UA_LogCategory category, const char* msg, va_list args);
void BoostLogSecondary(UA_LogLevel level, UA_LogCategory category, const char* msg, va_list args);

} // namespace opcuabridge_test_utils

#endif//OPCUABRIDGE_TEST_H_
