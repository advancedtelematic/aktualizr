#ifndef ASN1_CER_STREAM_H
#define ASN1_CER_STREAM_H

#include "asn1/asn1-cer.h"

#include <stack>

// tokens
namespace asn1 {

class Token {
 public:
  enum TokType { seq_tok, endseq_tok, restseq_tok, expl_tok, peekexpl_tok, endexpl_tok, opt_tok, endopt_tok };
  Token(TokType t) { type = t; }
  virtual ~Token() = default;
  TokType type;
};

class EndoptToken : public Token {
 public:
  EndoptToken(bool* result = nullptr) : Token(endopt_tok), result_p(result) {}
  ~EndoptToken() override = default;
  bool* result_p;
};

class ExplicitToken : public Token {
 public:
  ExplicitToken(uint8_t tag, ASN1_Class tag_class = kAsn1Context) : Token(expl_tok), tag(tag), tag_class(tag_class) {}
  ~ExplicitToken() override = default;
  uint8_t tag;
  ASN1_Class tag_class;
};

class PeekExplicitToken : public Token {
 public:
  PeekExplicitToken(uint8_t* tag = nullptr, ASN1_Class* tag_class = nullptr)
      : Token(peekexpl_tok), tag(tag), tag_class(tag_class) {}
  ~PeekExplicitToken() override = default;
  uint8_t* tag;
  ASN1_Class* tag_class;
};

const Token seq = Token(Token::seq_tok);
const Token endseq = Token(Token::endseq_tok);
const Token restseq = Token(Token::restseq_tok);
const Token endexpl = Token(Token::endexpl_tok);
const Token optional = Token(Token::opt_tok);
using endoptional = EndoptToken;
using peekexpl = PeekExplicitToken;
using expl = ExplicitToken;

template <ASN1_UniversalTag Tag, typename = void>
class ImplicitC {};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, int32_t&> {
 public:
  int32_t& data;
  ImplicitC(int32_t& d) : data(d) {}
};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, const int32_t&> {
 public:
  const int32_t& data;
  ImplicitC(const int32_t& d) : data(d) {}
};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, bool&> {
 public:
  bool& data;
  ImplicitC(bool& d) : data(d) {}
};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, const bool&> {
 public:
  const bool& data;
  ImplicitC(const bool& d) : data(d) {}
};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, std::string&> {
 public:
  std::string& data;
  ImplicitC(std::string& d) : data(d) {}
};

template <ASN1_UniversalTag Tag>
class ImplicitC<Tag, const std::string&> {
 public:
  const std::string& data;
  ImplicitC(const std::string& d) : data(d) {}
};

template <ASN1_UniversalTag Tag, typename T>
ImplicitC<Tag, T&> implicit(T& data) {
  return ImplicitC<Tag, T&>(data);
}

class Serializer {
 public:
  const std::string& getResult() { return result; }

  Serializer& operator<<(int32_t val);
  Serializer& operator<<(bool val);
  Serializer& operator<<(const std::string& val);
  Serializer& operator<<(ASN1_UniversalTag tag);
  Serializer& operator<<(const Token& tok);

 private:
  std::string result;
  ASN1_UniversalTag last_type{kUnknown};
};

template <ASN1_UniversalTag Tag, typename T>
Serializer& operator<<(Serializer& ser, ImplicitC<Tag, T> imp) {
  ser << Tag << imp.data;
  return ser;
}

class Deserializer {
 public:
  Deserializer(const std::string& d) : data(d){};

  Deserializer& operator>>(int32_t& val);
  Deserializer& operator>>(bool& val);
  Deserializer& operator>>(std::string& val);
  Deserializer& operator>>(ASN1_UniversalTag tag);
  Deserializer& operator>>(const Token& tok);

  const std::string& getData() { return data; }  // should only be used for testing

 private:
  std::string data;
  std::stack<int32_t> seq_lengths;
  std::stack<int32_t> seq_consumed;
  int32_t int_param{0};
  std::string string_param;

  int32_t opt_count{0};  // counter of tested asn1::optional
  bool opt_first{false};
  bool opt_present{false};
};

template <ASN1_UniversalTag Tag, typename T>
Deserializer& operator>>(Deserializer& des, ImplicitC<Tag, T> imp) {
  des >> Tag >> imp.data;
  return des;
}
}  // namespace asn1

#endif  // ASN1_CER_STREAM_H
