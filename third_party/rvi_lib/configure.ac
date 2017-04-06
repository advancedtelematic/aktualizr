AC_PREREQ([2.61])
AC_INIT([librvi], [0.1.0], [https://github.com/GENIVI/rvi_lib/issues])
AM_INIT_AUTOMAKE
LT_PREREQ([2.2])
LT_INIT([])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([src/config.h])

AC_SUBST([AM_CFLAGS], [-Wall -std=gnu99])

PKG_CHECK_MODULES([OPENSSL], [openssl >= 0.9.8])
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.0])
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], [true], [true])

# Call libjwt's ./configure script recursively
AC_CONFIG_SUBDIRS([libjwt])

AX_VALGRIND_CHECK

AX_CODE_COVERAGE

WEAK_ALIAS_SUPPORT

AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 examples/Makefile
 tests/Makefile
 src/librvi.pc
])

AC_OUTPUT
