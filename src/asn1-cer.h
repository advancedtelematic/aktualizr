#include <string>
#include <stdexcept>

// Limitations:
//   - Maximal supported integer width of 32 bits
//   - Maximal supported string length of 2**31 -1
//   - Only type tags up to 30 are supported

#define CER_MAX_PRIMITIVESTRING 1000 // defined in the standard

enum ASN1_Class {
  kAsn1Universal = 0x00,
};

enum ASN1_UniversalTag {
  kAsn1EndSequence = 0x00,
  kAsn1Sequence = 0x10,
  kAsn1Integer = 0x02,
  kAsn1OctetString = 0x04,
  kAsn1Enum = 0x0a,
  kAsn1Utf8String = 0x0c,
  kAsn1NumericString = 0x12,
  kAsn1PrintableString = 0x13,
  kAsn1TeletexString = 0x14,
  kAsn1VideotexString = 0x15,
  kAsn1UTCTime = 0x16,
  kAsn1GeneralizedTime = 0x17,
  kAsn1GraphicString = 0x18,
  kAsn1VisibleString = 0x19,
  kAsn1GeneralString = 0x1a,
  kAsn1UniversalString = 0x1b,
  kAsn1CharacterString = 0x1c,
  kAsn1BMPString = 0x1d,
};

enum ASN1_Token {
  kUnknown,
  kSequence,
  kInteger,
  kOctetString,
  kEndSequence,
};

class deserialization_error : public std::exception {
  const char * what () const throw ()
  {
    return "ASN.1 deserialization error";
  }
};

// Decode token. 
//    * ber - serialization
//    * endpos - next position in the string after what was deserialized
//    * int_param - integer value associated with token. Depends on token type.
//    * string_param - string associated with token. Depends on token type.
//  Return value: token type, or kUnknown in case of an error.

ASN1_Token cer_decode_token(const std::string& ber, int32_t* endpos, int32_t* int_param, std::string* string_param);

// Decode token, check if its type matches what is expected and return the rest of the string
//    * ber - serialization
//    * int_param - integer value associated with token. Depends on token type.
//    * string_param - string associated with token. Depends on token type.
//    * exp_type - expected token type.
//  Return value: rest of the string to parse
//  Throws: deserialization_error if type doesn't match
std::string cer_decode_except_crop(const std::string& ber, int32_t* int_param, std::string* string_param, ASN1_Token exp_type);

// Decode enum token with boudary check and crop the string
//    * ber - serialization
//    * value - enum value
//    * min - lower boundary
//    * max - higher boundary
//  Return value: rest of the string to parse
//  Throws: deserialization_error if type is not kInteger or boundary check was failed
template<typename T>
std::string cer_decode_except_crop_enum(const std::string& ber, T* value, int32_t min, int32_t max) {
  int32_t int_value;
  std::string res = cer_decode_except_crop(ber, &int_value, nullptr, kInteger);
  if(*value < min || *value > max)
    throw deserialization_error();

  *value = static_cast<T>(int_value);
  return res;
}
std::string cer_decode_except_crop_enum(const std::string& ber, int32_t* value, int32_t min, int32_t max);

std::string cer_encode_endcons();
std::string cer_encode_sequence();
std::string cer_encode_length(int32_t len);
std::string cer_encode_integer(int32_t number, ASN1_UniversalTag subtype);
std::string cer_encode_string(const std::string& contents, ASN1_UniversalTag subtype);
