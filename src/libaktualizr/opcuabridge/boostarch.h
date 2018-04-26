#ifndef OPCUABRIDGE_BOOST_ARCH_H_
#define OPCUABRIDGE_BOOST_ARCH_H_

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

#include <boost/archive/text_wiarchive.hpp>
#include <boost/archive/text_woarchive.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/tracking.hpp>

#include <boost/serialization/vector.hpp>

#define DATA_SERIALIZATION_OUT_BIN_STREAM boost::archive::binary_oarchive
#define DATA_SERIALIZATION_IN_BIN_STREAM boost::archive::binary_iarchive

#define DATA_SERIALIZATION_OUT_XML_STREAM boost::archive::xml_oarchive
#define DATA_SERIALIZATION_IN_XML_STREAM boost::archive::xml_iarchive

#define DATA_SERIALIZATION_OUT_TEXT_STREAM boost::archive::text_woarchive
#define DATA_SERIALIZATION_IN_TEXT_STREAM boost::archive::text_wiarchive

#endif  // OPCUABRIDGE_BOOST_ARCH_H_
