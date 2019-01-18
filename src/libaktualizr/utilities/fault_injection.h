// This file is a modified version of fiu-local.h, adapted to fit our style conventions
// + some custom additions

/* libfiu - Fault Injection in Userspace
 *
 * This header, part of libfiu, is meant to be included in your project to
 * avoid having libfiu as a mandatory build-time dependency.
 *
 * You can add it to your project, and #include it instead of fiu.h.
 * The real fiu.h will be used only when FIU_ENABLE is defined.
 *
 * This header, as the rest of libfiu, is in the public domain.
 *
 * You can find more information about libfiu at
 * http://blitiri.com.ar/p/libfiu.
 */

#ifndef _FAULT_INJECTION_H
#define _FAULT_INJECTION_H

/* Only define the stubs when fiu is disabled, otherwise use the real fiu.h
 * header */
#ifndef FIU_ENABLE

#define fiu_init(flags) 0
#define fiu_fail(name) 0
#define fiu_failinfo() NULL
#define fiu_do_on(name, action)
#define fiu_exit_on(name)
#define fiu_return_on(name, retval)

#define fault_injection_get_parameter(name) ""

#else

#include <fiu.h>
#include <stdlib.h>

static inline void fault_injection_set_parameter(const char *name, const char *value) { setenv(name, value, 1); }

static inline const char *fault_injection_get_parameter(const char *name) {
  const char *out = getenv(name);
  if (out == nullptr) {
    return "";
  }
  return getenv(name);
}

#endif /* FIU_ENABLE */

#endif /* _FAULT_INJECTION_H */
