#ifndef UTILS_H_
#define UTILS_H_


#include <string>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <boost/archive/iterators/transform_width.hpp>


using namespace boost::archive::iterators;

typedef base64_from_binary<transform_width<std::string::const_iterator, 6, 8> > base64_text;
typedef transform_width<binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6> base64_to_bin;


struct Utils{
    static std::string fromBase64(std::string);
    static std::string toBase64(std::string);
    
};

#endif //UTILS_H_