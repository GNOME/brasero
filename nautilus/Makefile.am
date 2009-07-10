module_flags = -avoid-version -module

INCLUDES =					\
	-I$(top_srcdir)/libbrasero-utils/	\
	-I$(top_builddir)/libbrasero-utils/	\
	-I$(top_srcdir)/libbrasero-media/	\
	-I$(top_builddir)/libbrasero-media/	\
	-I$(top_srcdir)/libbrasero-burn/	\
	-I$(top_builddir)/libbrasero-burn/	\
	-I$(top_srcdir)/src/			\
	-I$(top_builddir)/src/			\
	$(NAUTILUS_EXTENSION_CFLAGS)		\
	$(WERROR)				\
	-D_FILE_OFFSET_BITS=64			\
	-D_GNU_SOURCE				\
	-D_LARGEFILE64_SOURCE			\
	-D_REENTRANT				\
	-D_POSIX_PTHREAD_SEMANTICS		\
	-DPREFIX=\"$(prefix)\"			\
	-DBINDIR=\""$(bindir)"\"		\
	-DLIBEXECDIR=\""$(libexecdir)"\"	\
	-DSHAREDIR=\""$(datadir)"\"		\
	-DDATADIR=\""$(datadir)/brasero"\"	\
	$(DISABLE_DEPRECATED)

@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
desktop_in_files = brasero-nautilus.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(desktop_in_files)
CLEANFILES = $(desktop_DATA)
DISTCLEANFILES = $(NULL)

### Nautilus Extension
nautilus_extensiondir = $(NAUTILUSDIR)
nautilus_extension_LTLIBRARIES = libnautilus-brasero-extension.la

libnautilus_brasero_extension_la_SOURCES = 	\
	nautilus-burn-bar.c			\
	nautilus-burn-bar.h			\
	nautilus-burn-extension.c			\
	$(NULL)                       \
	../src/brasero-project-name.c                       \
	../src/brasero-project-name.h

libnautilus_brasero_extension_la_LIBADD =	\
	$(NAUTILUS_EXTENSION_LIBS)		\
	../libbrasero-utils/libbrasero-utils.la	\
	../libbrasero-media/libbrasero-media.la	\
	../libbrasero-burn/libbrasero-burn.la
	
libnautilus_brasero_extension_la_LDFLAGS = $(module_flags)

-include $(top_srcdir)/git.mk
