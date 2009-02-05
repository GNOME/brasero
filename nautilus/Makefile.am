module_flags = -avoid-version -module

INCLUDES =					\
	-I$(top_srcdir)/libbrasero-media/	\
	-I$(top_builddir)/libbrasero-media/	\
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
	$(DISABLE_DEPRECATED)			\
	$(BRASERO_HAL_CFLAGS)

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
	$(NULL)

libnautilus_brasero_extension_la_LIBADD =	\
	$(NAUTILUS_EXTENSION_LIBS) ../libbrasero-media/libbrasero-media.la

libnautilus_brasero_extension_la_LDFLAGS = $(module_flags)
