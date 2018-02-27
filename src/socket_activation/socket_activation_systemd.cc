#include <systemd/sd-daemon.h>

#include "socket_activation.h"

namespace socket_activation {

int listen_fds_start = SD_LISTEN_FDS_START;

int listen_fds(int unset_environment) { return sd_listen_fds(unset_environment); }

int listen_fds_with_names(int unset_environment, char*** names) {
  return sd_listen_fds_with_names(unset_environment, names);
}
};
