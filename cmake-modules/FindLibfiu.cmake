if (LIBFIU_LIBRARY AND LIBFIU_INCLUDE_DIR)
  # in cache already
  set(LIBFIU_FOUND TRUE)
else (LIBFIU_LIBRARY AND LIBFIU_INCLUDE_DIR)
  find_package(PkgConfig QUIET)
  if (PKG_CONFIG_FOUND)
      pkg_check_modules(libfiu_PKG QUIET fiu)
      set(XPREFIX libfiu_PKG)
  endif()

  find_path(LIBFIU_INCLUDE_DIR
    NAMES sd-daemon.h
    HINTS ${${XPREFIX}_INCLUDE_DIRS}
    PATH_SUFFIXES fiu)
  find_library(LIBFIU_LIBRARY
    NAMES ${${XPREFIX}_LIBRARIES} fiu
    HINTS ${${XPREFIX}_LIBRARY_DIRS}}
    PATH_SUFFIXES fiu)
endif()
