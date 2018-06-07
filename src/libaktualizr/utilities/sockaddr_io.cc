#include "utilities/sockaddr_io.h"
#include "utilities/utils.h"

std::ostream &operator<<(std::ostream &os, const sockaddr_storage &saddr) {
  os << Utils::ipDisplayName(saddr) << ":" << Utils::ipPort(saddr);
  return os;
}