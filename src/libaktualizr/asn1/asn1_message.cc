#include "asn1/asn1_message.h"

int Asn1StringAppendCallback(const void* buffer, size_t size, void* priv) {
  auto out_str = static_cast<std::string*>(priv);
  out_str->append(std::string(static_cast<const char*>(buffer), size));
  return 0;
}

std::string ToString(const OCTET_STRING_t& octet_str) {
  return std::string(reinterpret_cast<const char*>(octet_str.buf), octet_str.size);
}
