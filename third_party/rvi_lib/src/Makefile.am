# Include libjwt's header when compiling this directory

lib_LTLIBRARIES = librvi.la
librvi_la_SOURCES = btree.c rvi_list.c rvi.c
librvi_la_LDFLAGS = -version-info 0:1:0 
librvi_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libjwt/include $(OPENSSL_CFLAGS) -Wall
librvi_la_CFLAGS = -std=gnu99 -Wall 
librvi_la_LIBADD = $(JANSSON_LIBS) $(OPENSSL_LIBS) $(top_srcdir)/libjwt/libjwt/libjwt.la

pkgconfiglibdir = $(libdir)/pkgconfig
pkgconfiglib_DATA = librvi.pc
