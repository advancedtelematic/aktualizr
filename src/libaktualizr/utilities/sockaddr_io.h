#ifndef UTILITIES_SOCKADDR_IO_H_
#define UTILITIES_SOCKADDR_IO_H_

#include <sys/socket.h>
#include <iostream>

std::ostream &operator<<(std::ostream &os, const sockaddr_storage &saddr);

#endif  // UTILITIES_SOCKADDR_IO_H_