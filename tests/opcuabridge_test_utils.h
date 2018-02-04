#ifndef OPCUABRIDGE_TEST_H_
#define OPCUABRIDGE_TEST_H_

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

#endif//OPCUABRIDGE_TEST_H_
