#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "utils.h"

std::string getFreePort(){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
      std::cout << "socket() failed: " << errno;
      throw std::runtime_error("could not open socket");
    }
    struct sockaddr_in soc_addr;
    memset(&soc_addr, 0, sizeof(struct sockaddr_in));
    soc_addr.sin_family = AF_INET;
    soc_addr.sin_addr.s_addr = INADDR_ANY;
    soc_addr.sin_port = htons(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&soc_addr, sizeof(soc_addr)) == -1) {
      std::cout << "bind() failed: " << errno;
      throw std::runtime_error("could not pind socket");
    }

    struct sockaddr_in sa;
    unsigned int sa_len = sizeof(sa);
    if (getsockname(s, (struct sockaddr*)&sa, &sa_len) == -1) {
      throw std::runtime_error("getsockname fails");
    }
    close(s);
    return Utils::intToString(ntohs(sa.sin_port));
}

#endif // TEST_UTILS_H_