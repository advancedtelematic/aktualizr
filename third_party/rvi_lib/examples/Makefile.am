EXAMPLES = \
	interactive 

examplesdir = $(top_srcdir)/examples

examples_PROGRAMS = $(EXAMPLES)

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall $(CHECK_CFLAGS) -std=gnu99 -D_GNU_SOURCE
AM_LDFLAGS = -L$(top_builddir)/src/.libs
LDADD = -lrvi -ldl 

