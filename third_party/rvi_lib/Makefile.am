ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libjwt include src . examples tests

dist_doc_DATA = README.md

#check-valgrind: all
#	$(MAKE) $(AM_MAKEFLAGS) -C tests check-valgrind

#check-code-coverage: all
#	$(MAKE) $(AM_MAKEFLAGS) -C tests check-code-coverage

docs:
	doxygen
