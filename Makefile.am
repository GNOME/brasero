## Process this file with automake to produce Makefile.in.
SUBDIRS = src po data docs

SUBDIRS += help

if BUILD_NAUTILUS
SUBDIRS += nautilus
endif

braserodocdir = ${prefix}/docs/brasero


EXTRA_DIST = 			\
	AUTHORS			\
	MAINTAINERS		\
	NEWS			\
	README			\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	gnome-doc-utils.make	\
	m4

noinst_PROGRAMS =

DIST_CHECK_CONFIGRE_FLAGS =
	--disable-scrollkeeper

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update		\
	gtk-doc.make

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done
