#include "asn1-cerstream.h"

namespace asn1 {

Serializer& Serializer::operator<<(int32_t val) {
  result.append(cer_encode_integer(val));
  return *this;
}

Serializer& Serializer::operator<<(bool val) {
  result.append(cer_encode_integer(static_cast<int32_t>(val)));
  return *this;
}

Serializer& Serializer::operator<<(const std::string& val) {
  result.append(cer_encode_string(val, last_type));
  return *this;
}

Serializer& Serializer::operator<<(ASN1_UniversalTag tag) {
  // only support primitive universal tags, sequence serialized with tokens
  result.push_back(static_cast<char>(tag & 0xFF));
  last_type = tag;
  return *this;
}

Serializer& Serializer::operator<<(const Token& tok) {
  switch (tok.type) {
    case Token::seq_tok:
      result.push_back(0x30);
      result.push_back(static_cast<char>(0x80));
      break;
    case Token::endseq_tok:
    case Token::endexpl_tok:
      result.push_back(0x00);
      result.push_back(0x00);
      break;
    case Token::expl_tok: {
      auto full_tag = static_cast<uint8_t>(dynamic_cast<const ExplicitToken&>(tok).tag |
                                           dynamic_cast<const ExplicitToken&>(tok).tag_class);
      result.push_back(static_cast<char>(full_tag | 0x20));  // set 'constructed' bit
      result.push_back(static_cast<char>(0x80));
      break;
    }
    default:
      throw std::runtime_error("Unknown token type in ASN1 serialization");
  }
  return *this;
}

Deserializer& Deserializer::operator>>(int32_t& val) {
  if ((opt_count == 0) || opt_present) {
    val = int_param;
  }
  return *this;
}

Deserializer& Deserializer::operator>>(bool& val) {
  if ((opt_count == 0) || opt_present) {
    val = (int_param != 0);
  }
  return *this;
}

Deserializer& Deserializer::operator>>(std::string& val) {
  if ((opt_count == 0) || opt_present) {
    val = string_param;
  }
  return *this;
}

Deserializer& Deserializer::operator>>(ASN1_UniversalTag tag) {
  int32_t endpos = 0;
  if ((opt_count != 0) && !opt_present && !opt_first) {
    return *this;
  }

  if (tag != cer_decode_token(data, &endpos, &int_param, &string_param)) {
    // failed to read first tag from OPTIONAL => OPTIONAL not present
    if ((opt_count != 0) && opt_first) {
      opt_present = false;
      opt_first = false;
      return *this;
    }

    throw deserialization_error();
  }
  if ((opt_count != 0) && opt_first) {
    opt_present = true;
    opt_first = false;
  }
  if (!seq_lengths.empty() && seq_lengths.top() > 0) {
    seq_consumed.top() += endpos;
    if (seq_consumed.top() > seq_lengths.top()) {
      throw deserialization_error();
    }
  }
  data = data.substr(static_cast<size_t>(endpos));
  return *this;
}

Deserializer& Deserializer::operator>>(const Token& tok) {
  int32_t endpos;
  int32_t seq_len;

  if ((opt_count != 0) && !opt_present && !opt_first) {
    if (tok.type == Token::endopt_tok) {
      --opt_count;
      bool* result = dynamic_cast<const EndoptToken&>(tok).result_p;
      if (result != nullptr) {
        *result = opt_present;
      }
    }
    return *this;
  }

  switch (tok.type) {
    case Token::seq_tok:
      if (kAsn1Sequence != cer_decode_token(data, &endpos, &seq_len, nullptr)) {
        if ((opt_count != 0) && opt_first) {
          opt_present = false;
          opt_first = false;
          break;
        }

        throw deserialization_error();
      }
      if ((opt_count != 0) && opt_first) {
        opt_present = true;
        opt_first = false;
      }

      data = data.substr(static_cast<size_t>(endpos));
      seq_lengths.push(seq_len);
      seq_consumed.push(0);
      break;

    case Token::endseq_tok:
    case Token::endexpl_tok: {
      int32_t len = seq_lengths.top();
      int32_t cons_len = seq_consumed.top();
      seq_lengths.pop();
      seq_consumed.pop();
      if (len >= 0) {
        if (cons_len != len) {
          throw deserialization_error();
        }
      } else {
        if (kAsn1EndSequence != cer_decode_token(data, &endpos, nullptr, nullptr)) {
          throw deserialization_error();
        }
        data = data.substr(static_cast<size_t>(endpos));
        cons_len += endpos;
      }

      // add total length of current sequence to consumed bytes of the parent sequence
      if (!seq_lengths.empty()) {
        seq_consumed.top() += cons_len + 2;
      }
      break;
    }

    case Token::restseq_tok: {
      int32_t len = seq_lengths.top();
      int32_t cons_len = seq_consumed.top();
      seq_lengths.pop();
      seq_consumed.pop();
      if (len >= 0) {
        while (len > cons_len) {
          if (cer_decode_token(data, &endpos, nullptr, nullptr) == kUnknown) {
            throw deserialization_error();
          }
          data = data.substr(static_cast<size_t>(endpos));
          cons_len += endpos;
        }
        if (cons_len != len) {
          throw deserialization_error();
        }
      } else {
        int nestedness = 0;
        int32_t decoded_int = 0;

        for (;;) {
          if (data.empty()) {  // end of data before end of sequence
            throw deserialization_error();
          }
          uint8_t ret = cer_decode_token(data, &endpos, &decoded_int, nullptr);
          data = data.substr(static_cast<size_t>(endpos));
          cons_len += endpos;

          if (ret == kUnknown) {
            throw deserialization_error();
          }
          if (ret == kAsn1Sequence) {  // TODO: explicit tags (constructed tokens in general)
            // indefininte length, expect an end of sequence marker
            if (decoded_int == -1) {
              ++nestedness;
            }
          } else if (ret == kAsn1EndSequence) {
            if (nestedness == 0) {
              break;
            }
            --nestedness;
          }
        }
      }
      if (!seq_lengths.empty()) {
        seq_consumed.top() += cons_len + 2;
      }
      break;
    }

    case Token::expl_tok: {
      auto full_tag = static_cast<uint8_t>(dynamic_cast<const ExplicitToken&>(tok).tag |
                                           dynamic_cast<const ExplicitToken&>(tok).tag_class);
      if (full_tag != cer_decode_token(data, &endpos, &seq_len, nullptr)) {
        if ((opt_count != 0) && opt_first) {
          opt_present = false;
          opt_first = false;
          break;
        }
        throw deserialization_error();
      }
      if ((opt_count != 0) && opt_first) {
        opt_present = true;
        opt_first = false;
      }

      data = data.substr(static_cast<size_t>(endpos));
      seq_lengths.push(seq_len);
      seq_consumed.push(0);
      break;
    }

    case Token::peekexpl_tok: {
      uint8_t full_tag = cer_decode_token(data, &endpos, &seq_len, nullptr);
      if (full_tag == kUnknown) {
        throw deserialization_error();
      }
      if ((opt_count != 0) && opt_first) {
        opt_present = true;
        opt_first = false;
      }

      uint8_t* out_tag = dynamic_cast<const PeekExplicitToken&>(tok).tag;
      ASN1_Class* out_tag_class = dynamic_cast<const PeekExplicitToken&>(tok).tag_class;
      if (out_tag != nullptr) {
        *out_tag = full_tag & 0x1F;
      }
      if (out_tag_class != nullptr) {
        *out_tag_class = static_cast<ASN1_Class>(full_tag & 0xC0);
      }

      data = data.substr(static_cast<size_t>(endpos));
      seq_lengths.push(seq_len);
      seq_consumed.push(0);
      break;
    }

    case Token::opt_tok:
      ++opt_count;
      opt_first = true;
      break;

    case Token::endopt_tok: {
      --opt_count;
      bool* result = dynamic_cast<const EndoptToken&>(tok).result_p;
      if (result != nullptr) {
        *result = opt_present;
      }
      break;
    }

    default:
      throw std::runtime_error("Unknown token type in ASN1 serialization");
  }
  // seq_lengths.pop();
  // seq_consumed.pop();
  return *this;
}

}  // namespace asn1
