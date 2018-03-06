#include "asn1-cer.h"

#include <stack>

// tokens
namespace asn1 {

class Token 
{
  public:
    enum TokType {seq_tok, endseq_tok, restseq_tok};
    Token(TokType t) {type = t;}
    TokType type;
};

const Token seq = Token(Token::seq_tok);
const Token endseq = Token(Token::endseq_tok);
const Token restseq = Token(Token::restseq_tok);

class Serializer {
  public:
    const std::string& getResult() {return result;}

    Serializer& operator << (int32_t val);
    Serializer& operator << (std::string val);
    Serializer& operator << (ASN1_UniversalTag tag);
    Serializer& operator << (Token tok);
  private:
    std::string result;
    ASN1_UniversalTag last_type;
};

class Deserializer {
  public:
    Deserializer(const std::string& d) {data = d;};

    Deserializer& operator >> (int32_t& val);
    Deserializer& operator >> (std::string& val);
    Deserializer& operator >> (ASN1_UniversalTag tag);
    Deserializer& operator >> (Token tok);
  private:
    std::string data;
    ASN1_UniversalTag last_type;
    std::stack<int32_t> seq_lengths;
    std::stack<int32_t> seq_consumed;
    int32_t int_param;
    std::string string_param;
};

}
