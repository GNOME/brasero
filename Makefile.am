## Process this file with automake to produce Makefile.in.
SUBDIRS = src po data docs

braserodocdir = ${prefix}/docs/brasero


EXTRA_DIST = 			\
	AUTHORS			\
	HACKING			\
	MAINTAINERS		\
	NEWS			\
	README			\
	acinclude.m4

noinst_PROGRAMS =

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done
