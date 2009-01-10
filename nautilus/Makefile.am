module_flags = -avoid-version -module

INCLUDES =					\
	$(NAUTILUS_EXTENSION_CFLAGS)		\
	$(BRASERO_DBUS_CFLAGS)			\
	$(BRASERO_HAL_CFLAGS)			\
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


CLEANFILES = 			\
	$(NULL)

DISTCLEANFILES = 		\
	$(NULL)

### Nautilus Extension
nautilus_extensiondir = $(NAUTILUSDIR)
nautilus_extension_LTLIBRARIES = libnautilus-brasero-extension.la

libnautilus_brasero_extension_la_SOURCES = 	\
	nautilus-burn-bar.c			\
	nautilus-burn-bar.h			\
	nautilus-burn-extension.c			\
	$(NULL)

libnautilus_brasero_extension_la_LIBADD =	\
	$(NAUTILUS_EXTENSION_LIBS)		\
	$(BRASERO_DBUS_LIBS)			\
	$(BRASERO_HAL_LIBS)

libnautilus_brasero_extension_la_LDFLAGS = $(module_flags)
