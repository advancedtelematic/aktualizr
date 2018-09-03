# Try to find OSTree
# OSTREE_DIR - Hint path to a local build of OSTree
# Once done this will define
#  LIBOSTREE_FOUND
#  LIBOSTREE_INCLUDE_DIRS
#  LIBOSTREE_LIBRARIES
#

# OSTree
find_path(LIBOSTREE_INCLUDE_DIR
    NAMES ostree.h
    HINTS ${OSTREE_DIR}/include
    PATH_SUFFIXES ostree-1 src/libostree)

find_library(LIBOSTREE_LIBRARY
    NAMES libostree-1.so
    HINTS ${OSTREE_DIR}/lib
    PATH_SUFFIXES /.libs)

message("libostree headers path: ${LIBOSTREE_INCLUDE_DIR}")
message("libostree library path: ${LIBOSTREE_LIBRARY}")

# glib
find_package(PkgConfig)

pkg_check_modules(OSTREE_GLIB REQUIRED gio-2.0 glib-2.0)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OSTree DEFAULT_MSG
    LIBOSTREE_LIBRARY LIBOSTREE_INCLUDE_DIR OSTREE_GLIB_FOUND)

mark_as_advanced(LIBOSTREE_INCLUDE_DIR LIBOSTREE_LIBRARY)

set(LIBOSTREE_INCLUDE_DIRS ${LIBOSTREE_INCLUDE_DIR} ${OSTREE_GLIB_INCLUDE_DIRS})
set(LIBOSTREE_LIBRARIES ${LIBOSTREE_LIBRARY} ${OSTREE_GLIB_LIBRARIES})
