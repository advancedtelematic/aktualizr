#ifndef SOCKET_ACTIVATION_H_
#define SOCKET_ACTIVATION_H_

namespace socket_activation {

// maps the interface of <systemd/sd-daemon.h>

extern int listen_fds_start;

int listen_fds(int unset_environment);
int listen_fds_with_names(int unset_environment, char*** names);
};

#endif  // SOCKET_ACTIVATION_H_
