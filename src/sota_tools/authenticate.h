#ifndef SOTA_CLIENT_TOOLS_AUTHENTICATE_H_
#define SOTA_CLIENT_TOOLS_AUTHENTICATE_H_

#include <string>

#include "treehub_server.h"

int authenticate(const std::string &cacerts, std::string filepath,
                 TreehubServer &treehub);

// vim: set tabstop=2 shiftwidth=2 expandtab:
#endif  // SOTA_CLIENT_TOOLS_AUTHENTICATE_H_
