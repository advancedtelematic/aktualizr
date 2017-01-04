#include "logging.h"

long get_curlopt_verbose() { return logging_verbosity > 0 ? 1L : 0L; }

// vim: set tabstop=2 shiftwidth=2 expandtab:
