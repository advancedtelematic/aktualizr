if (SYSTEMD_LIBRARY AND SYSTEMD_INCLUDE_DIR)
  # in cache already
  set(SYSTEMD_FOUND TRUE)
else (SYSTEMD_LIBRARY AND SYSTEMD_INCLUDE_DIR)
  find_package(PkgConfig QUIET)
  if (PKG_CONFIG_FOUND)
      pkg_check_modules(systemd_PKG QUIET systemd)
      set(XPREFIX systemd_PKG)
  endif()

  find_path(SYSTEMD_INCLUDE_DIR
    NAMES sd-daemon.h
    HINTS ${${XPREFIX}_INCLUDE_DIRS}
    PATH_SUFFIXES systemd)
  find_library(SYSTEMD_LIBRARY
    NAMES ${${XPREFIX}_LIBRARIES} systemd
    HINTS ${${XPREFIX}_LIBRARY_DIRS}}
    PATH_SUFFIXES systemd)
endif()
