#include "asn1-cerstream.h"

namespace asn1 {

Serializer& Serializer::operator<<(int32_t val) {
  result.append(cer_encode_integer(val));
  return *this;
}

Serializer& Serializer::operator<<(std::string val) {
  result.append(cer_encode_string(val, last_type));
  return *this;
}

Serializer& Serializer::operator<<(ASN1_UniversalTag tag) {
  result.push_back(tag & 0xFF);  // only support primitive universal tags, sequence serialized with tokens
  last_type = tag;
  return *this;
}

Serializer& Serializer::operator<<(Token tok) {
  switch (tok.type) {
    case Token::seq_tok:
      result.push_back(0x30);
      result.push_back(0x80);
      break;
    case Token::endseq_tok:
      result.push_back(0x00);
      result.push_back(0x00);
      break;
    default:
      throw std::runtime_error("Unknown token type in ASN1 serialization");
  }
  return *this;
}

Deserializer& Deserializer::operator>>(int32_t& val) {
  val = int_param;
  return *this;
}

Deserializer& Deserializer::operator>>(std::string& val) {
  val = string_param;
  return *this;
}

Deserializer& Deserializer::operator>>(ASN1_UniversalTag tag) {
  int32_t endpos = 0;
  if (tag != cer_decode_token(data, &endpos, &int_param, &string_param)) throw deserialization_error();
  if (!seq_lengths.empty() && seq_lengths.top() > 0) {
    seq_consumed.top() += endpos;
    if (seq_consumed.top() > seq_lengths.top()) throw deserialization_error();
  }
  data = data.substr(endpos);
  return *this;
}

Deserializer& Deserializer::operator>>(Token tok) {
  int32_t endpos;
  int32_t seq_len;

  switch (tok.type) {
    case Token::seq_tok:
      if (kAsn1Sequence != cer_decode_token(data, &endpos, &seq_len, nullptr)) throw deserialization_error();
      data = data.substr(endpos);
      seq_lengths.push(seq_len);
      seq_consumed.push(0);
      break;

    case Token::endseq_tok: {
      int32_t len = seq_lengths.top();
      int32_t cons_len = seq_consumed.top();
      seq_lengths.pop();
      seq_consumed.pop();
      if (len > 0) {
        if (cons_len != len) throw deserialization_error();
      } else {
        if (kAsn1EndSequence != cer_decode_token(data, &endpos, nullptr, nullptr)) throw deserialization_error();
        data = data.substr(endpos);
        cons_len += endpos;
      }

      // add total length of current sequence to consumed bytes of the parent sequence
      if (!seq_lengths.empty()) seq_lengths.top() += cons_len + 2;
    }

    case Token::restseq_tok:
      break;
      int32_t len = seq_lengths.top();
      if (len > 0) {
        while (len > seq_consumed.top()) {
          if (cer_decode_token(data, &endpos, nullptr, nullptr) == kUnknown) throw deserialization_error();
          data = data.substr(endpos);
          seq_consumed.top() += endpos;
        }
        if (seq_consumed.top() != len) throw deserialization_error();
      } else {
        int nestedness = 0;
        int32_t int_param = 0;

        for (;;) {
          if (data.empty())  // end of data before end of sequence
            throw deserialization_error();
          ASN1_UniversalTag ret = cer_decode_token(data, &endpos, &int_param, nullptr);
          data = data.substr(endpos);

          if (ret == kUnknown) {
            throw deserialization_error();
          } else if (ret == kAsn1Sequence) {
            // indefininte length, expect an end of sequence marker
            if (int_param == -1) ++nestedness;
          } else if (ret == kAsn1EndSequence) {
            if (nestedness == 0)
              break;
            else
              --nestedness;
          }
        }
      }
  }
  seq_lengths.pop();
  seq_consumed.pop();
  return *this;
}
}
