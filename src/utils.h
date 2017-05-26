#ifndef UTILS_H_
#define UTILS_H_


#include <string>
#include <boost/filesystem.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>

#include "json/json.h"


using namespace boost::archive::iterators;

typedef base64_from_binary<transform_width<std::string::const_iterator, 6, 8> > base64_text;
typedef transform_width<binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6> base64_to_bin;


struct Utils{
    static std::string fromBase64(std::string);
    static std::string toBase64(std::string);
    static Json::Value parseJSON(const std::string &json_str);
    static Json::Value parseJSONFile(const boost::filesystem::path & filename);
    static std::string intToString(unsigned int val);

    
};

#endif //UTILS_H_