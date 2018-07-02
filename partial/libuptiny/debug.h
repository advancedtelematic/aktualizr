#ifndef LIBUPTINY_DEBUG_H
#define LIBUPTINY_DEBUG_H

#ifdef DEBUG
#  include <stdio.h>
#  define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#  define DEBUG_PRINTF(...) {;}
#endif // DEBUG

#endif // LIBUPTINY_DEBUG_H
