#include "socket_activation.h"

namespace socket_activation {

int listen_fds_start = 3;

int listen_fds(int unset_environment) {
  (void)unset_environment;

  return 0;
}

int listen_fds_with_names(int unset_environment, char*** names) {
  (void)unset_environment;
  (void)names;

  return 0;
}

};
