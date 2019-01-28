#include "asn1/asn1-cer.h"
#include <algorithm>

static std::string cer_encode_length(size_t len) {
  std::string res;
  // 1-byte length
  if (len <= 127) {
    res.push_back(static_cast<char>(len));
    return res;
  }

  // multibyte length
  do {
    res.push_back(static_cast<char>(len & 0xFF));
    len >>= 8;
  } while (len != 0);

  res.push_back(static_cast<char>(res.length() | 0x80));
  std::reverse(res.begin(), res.end());

  return res;
}

// shifting signed integers right is UB, make sure it's filled with 1s
constexpr int32_t shr8(int32_t arg) {
  return static_cast<int32_t>((static_cast<uint32_t>(arg) >> 8) | ((arg < 0) ? 0xff000000 : 0x00000000));
}

std::string cer_encode_integer(int32_t number) {
  std::string res;

  do {
    res.push_back(static_cast<char>(number & 0xFF));
    number = shr8(number);
  } while ((number != 0) && (static_cast<uint32_t>(number) != 0xffffffff));

  if (((res[0] & 0x80) != 0) && (number == 0)) {
    res.push_back(0x00);
  } else if (((res[0] & 0x80) == 0) && (static_cast<uint32_t>(number) == 0xffffffff)) {
    res.push_back(static_cast<char>(0xFF));
  }

  res.push_back(static_cast<char>(res.length()));
  std::reverse(res.begin(), res.end());
  return res;
}

std::string cer_encode_string(const std::string& contents, ASN1_UniversalTag tag) {
  size_t len = contents.length();

  std::string res;

  if (len <= CER_MAX_PRIMITIVESTRING) {
    res += cer_encode_length(len);
    res += contents;
    return res;
  }

  res.push_back(static_cast<char>(0x80));
  std::string contents_copy = contents;
  while (!contents_copy.empty()) {
    res.push_back(static_cast<char>(tag));
    size_t chunk_size =
        (contents_copy.length() > CER_MAX_PRIMITIVESTRING) ? CER_MAX_PRIMITIVESTRING : contents_copy.length();
    res += cer_encode_string(contents_copy.substr(0, chunk_size), tag);
    contents_copy = contents_copy.substr(chunk_size);
  }
  res.push_back(0x00);
  res.push_back(0x00);
  return res;
}

static int32_t cer_decode_length(const std::string& content, int32_t* endpos) {
  if ((static_cast<uint8_t>(content[0])) == 0x80) {
    *endpos = 1;
    return -1;
  }

  if (((static_cast<uint8_t>(content[0])) & 0x80) == 0) {
    *endpos = 1;
    return content[0];
  }

  int32_t len_len = (static_cast<uint8_t>(content[0])) & 0x7F;
  *endpos = len_len + 1;
  if (len_len > 4) {
    return -2;
  }

  int32_t res = 0;
  for (uint32_t i = 0; i < static_cast<uint32_t>(len_len); i++) {
    res <<= 8;
    res |= static_cast<int32_t>(content[i + 1UL] & 0xFF);
  }

  // In case of overflow number can accidentially take a 'special' value (only -1 now). Make sure it is interpreted as
  // error.
  if (res < 0) {
    res = -2;
  }

  return res;
}

uint8_t cer_decode_token(const std::string& ber, int32_t* endpos, int32_t* int_param, std::string* string_param) {
  *endpos = 0;
  if (ber.length() < 2) {
    return kUnknown;
  }

  auto type_class = static_cast<ASN1_Class>(ber[0] & 0xC0);
  auto tag = static_cast<ASN1_UniversalTag>(ber[0] & 0x1F);
  bool constructed = !((ber[0] & 0x20) == 0);
  int32_t len_endpos;
  int32_t token_len = cer_decode_length(ber.substr(1), &len_endpos);

  // token_len of -1 is used as indefinite length marker
  if (token_len < -1) {
    return kUnknown;
  }

  std::string content;
  if (token_len == -1) {  // indefinite form, take the whole tail
    content = ber.substr(2);
  } else {  // definite form
    content = ber.substr(static_cast<uint32_t>(1L + len_endpos), static_cast<uint32_t>(token_len));
  }

  if (type_class == kAsn1Universal) {
    switch (tag) {
      case kAsn1Sequence:
        if (!constructed) {
          return kUnknown;
        }

        if (int_param != nullptr) {
          *int_param = token_len;
        }
        *endpos = len_endpos + 1;
        return kAsn1Sequence;

      case kAsn1EndSequence:
        if (token_len != 0) {
          return kUnknown;
        } else {
          return kAsn1EndSequence;
        }

      case kAsn1Integer:
      case kAsn1Boolean:
      case kAsn1Enum: {
        if (constructed || token_len == -1) {
          return kUnknown;
        }

        // support max. 32 bit-wide integers
        if (content.length() > 4 || content.length() < 1) {
          return kUnknown;
        }

        bool sign = !((content[0] & 0x80) == 0);

        int32_t res = 0;
        for (size_t i = 0; i < content.length(); i++) {
          res <<= 8;
          res |= content[i];
        }

        if (sign) {
          for (int i = token_len; i < 4; i++) {
            res |= (0xff << (i << 3));
          }
        }

        if (int_param != nullptr) {
          *int_param = res;
        }
        *endpos = 1 + len_endpos + token_len;
        return tag;
      }
      case kAsn1OctetString:
      case kAsn1Utf8String:
      case kAsn1NumericString:
      case kAsn1PrintableString:
      case kAsn1TeletexString:
      case kAsn1VideotexString:
      case kAsn1UTCTime:
      case kAsn1GeneralizedTime:
      case kAsn1GraphicString:
      case kAsn1VisibleString:
      case kAsn1GeneralString:
      case kAsn1UniversalString:
      case kAsn1CharacterString:
      case kAsn1BMPString: {
        if (token_len >= 0) {  // Fixed length encoding
          if (string_param != nullptr) {
            *string_param = content;
          }
          *endpos = 1 + len_endpos + token_len;
        } else {
          int32_t position = 1 + len_endpos;
          if (string_param != nullptr) {
            *string_param = std::string();
          }
          for (;;) {
            int32_t internal_endpos;
            std::string internal_string_param;
            uint8_t token = cer_decode_token(ber.substr(static_cast<size_t>(position)), &internal_endpos, nullptr,
                                             &internal_string_param);
            if (token == kAsn1EndSequence) {
              return tag;
            }
            if (token != tag) {
              return kUnknown;
            }

            // common case: a string segment
            if (string_param != nullptr) {
              *string_param += internal_string_param;
            }
            position += internal_endpos;
          }
          *endpos = position;
        }
        return tag;
      }
      default:
        return kUnknown;
    }
  } else {  // assume explicit tag, return full tag value except constructed bit
    if (!constructed) {
      return kUnknown;
    }

    if (int_param != nullptr) {
      *int_param = token_len;
    }
    *endpos = len_endpos + 1;

    return static_cast<uint8_t>(static_cast<uint8_t>(tag) | static_cast<uint8_t>(type_class));
  }
}
