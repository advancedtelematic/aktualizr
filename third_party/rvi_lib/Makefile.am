ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libjwt include src examples

dist_doc_DATA = README.md

examplesdir = examples

check-valgrind: all
	$(MAKE) $(AM_MAKEFLAGS) -C examples check-valgrind

check-code-coverage: all
	$(MAKE) $(AM_MAKEFLAGS) -C examples check-code-coverage

docs:
	doxygen
