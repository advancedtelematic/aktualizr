#include "asn1-cer.h"

#include <stack>

// tokens
namespace asn1 {

class Token {
 public:
  enum TokType { seq_tok, endseq_tok, restseq_tok };
  Token(TokType t) { type = t; }
  TokType type;
};

const Token seq = Token(Token::seq_tok);
const Token endseq = Token(Token::endseq_tok);
const Token restseq = Token(Token::restseq_tok);

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
  Serializer& operator<<(std::string val);
  Serializer& operator<<(ASN1_UniversalTag tag);
  Serializer& operator<<(Token tok);

 private:
  std::string result;
  ASN1_UniversalTag last_type;
};

template <ASN1_UniversalTag Tag, typename T>
Serializer& operator<<(Serializer& ser, ImplicitC<Tag, T> imp) {
  ser << Tag << imp.data;
  return ser;
}

class Deserializer {
 public:
  Deserializer(const std::string& d) { data = d; };

  Deserializer& operator>>(int32_t& val);
  Deserializer& operator>>(std::string& val);
  Deserializer& operator>>(ASN1_UniversalTag tag);
  Deserializer& operator>>(Token tok);

 private:
  std::string data;
  ASN1_UniversalTag last_type;
  std::stack<int32_t> seq_lengths;
  std::stack<int32_t> seq_consumed;
  int32_t int_param;
  std::string string_param;
};

template <ASN1_UniversalTag Tag, typename T>
Deserializer& operator>>(Deserializer& des, ImplicitC<Tag, T> imp) {
  des >> Tag >> imp.data;
  return des;
}
}
