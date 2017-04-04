TESTS = \
	check_init 

check_PROGRAMS = $(TESTS) 

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall $(CHECK_CFLAGS) -DKEYDIR="\"$(srcdir)/keys\"" -D_GNU_SOURCE
AM_LDFLAGS = -L$(top_builddir)/src
LDADD = -lrvi $(CHECK_LIBS)
