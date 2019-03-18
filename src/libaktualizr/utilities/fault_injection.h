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

// Note: was `#define fault_injection_last_info() ""` but it triggers
static inline std::string fault_injection_last_info() { return ""; }

#else

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include <string>

#include <fiu-control.h>
#include <fiu.h>

static constexpr size_t fault_injection_info_bs = 256;

static inline const char *fault_injection_info_fn() {
  static std::mutex mutex;
  static char info_fn[128];

  std::lock_guard<std::mutex> lock(mutex);

  if (info_fn[0] != '\0') {
    return info_fn;
  }

  snprintf(info_fn, sizeof(info_fn), "/tmp/aktualizr-faults-info-%lu", static_cast<unsigned long>(getpid()));

  return info_fn;
}

static inline std::string fault_injection_last_info() {
  auto info_id = reinterpret_cast<unsigned long>(fiu_failinfo());

  std::array<char, fault_injection_info_bs> arr{};
  size_t offset = info_id * fault_injection_info_bs;
  std::ifstream f;
  f.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  try {
    f.open(fault_injection_info_fn(), std::ios::binary);
    f.seekg(offset);
    f.get(arr.data(), arr.size());

    std::string info(arr.data());
    return info;
  } catch (std::ifstream::failure &e) {
    return "";
  }
}

// proxy for fiu_enable, with persisted failinfo (through a file)
static inline int fault_injection_enable(const char *name, int failnum, const std::string &failinfo,
                                         unsigned int flags) {
  std::array<char, fault_injection_info_bs> arr{};
  std::copy(failinfo.cbegin(), failinfo.cend(), arr.data());

  size_t failinfo_id = 0;

  if (failinfo != "") {
    std::ofstream f;
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
      f.open(fault_injection_info_fn(), std::ios::binary);
      f.seekp(0, std::ios_base::end);
      size_t fi_id = f.tellp() / fault_injection_info_bs;
      f.write(arr.data(), arr.size());
      failinfo_id = fi_id;
    } catch (std::ofstream::failure &e) {
    }
  }

  return fiu_enable(name, failnum, reinterpret_cast<void *>(failinfo_id), flags);
}

#define fault_injection_disable fiu_disable

#endif /* FIU_ENABLE */

#endif /* _FAULT_INJECTION_H */
