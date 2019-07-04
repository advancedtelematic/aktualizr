#ifndef AKTUALIZR_VERSION_H_
#define AKTUALIZR_VERSION_H_

// This is part of a separate library which is the only one where the
// AKTUALIZR_VERSION macro is defined, so that the rest of the code base
// compilation can be cached (e.g: with ccache)

const char *aktualizr_version();

#endif  // AKTUALIZR_VERSION_H_
