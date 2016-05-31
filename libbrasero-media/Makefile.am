AM_CPPFLAGS = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libbrasero-media				\
	-I$(top_builddir)						\
	-I$(top_builddir)/libbrasero-media				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GMODULE_EXPORT_CFLAGS)				\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libbrasero-marshal.h libbrasero-marshal.c

CLEANFILES =
EXTRA_DIST =

libbrasero-marshal.h: libbrasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --header > libbrasero-marshal.h )
libbrasero-marshal.c: libbrasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --body --header > libbrasero-marshal.c )

headerdir = $(includedir)/brasero3
header_HEADERS = \
	brasero-drive.h			\
	brasero-medium-monitor.h	\
	brasero-media.h			\
	brasero-units.h			\
	brasero-medium.h		\
	brasero-volume.h		\
	brasero-medium-selection.h	\
	brasero-drive-selection.h

lib_LTLIBRARIES = libbrasero-media3.la

libbrasero_media3_la_LIBADD = \
	$(BRASERO_GLIB_LIBS) 		\
	$(BRASERO_GMODULE_EXPORT_LIBS) 	\
	$(BRASERO_GTHREAD_LIBS)		\
	$(BRASERO_GIO_LIBS) 		\
	$(BRASERO_GTK_LIBS) 		\
	$(BRASERO_SCSI_LIBS)

libbrasero_media3_la_LDFLAGS =					\
	-version-info $(LIBBRASERO_LT_VERSION)			\
	-no-undefined
	$(AM_LDFLAGS)
#	-export-symbols ./libbrasero-media.symbols

libbrasero_media3_la_SOURCES =		\
	libbrasero-marshal.c		\
	libbrasero-marshal.h		\
	brasero-drive.h			\
	brasero-medium-monitor.h	\
	brasero-media.h			\
	brasero-units.h			\
	brasero-medium.h		\
	brasero-volume.h		\
	brasero-medium-selection.h	\
	scsi-base.h 			\
	scsi-command.h 			\
	scsi-error.h         		\
	scsi-get-configuration.c        \
	scsi-get-configuration.h        \
	scsi-mmc1.h         		\
	scsi-mmc2.h         		\
	scsi-opcodes.h         		\
	scsi-read-disc-info.c         	\
	scsi-read-disc-info.h         	\
	scsi-read-toc-pma-atip.c        \
	scsi-read-toc-pma-atip.h        \
	scsi-sense-data.c         	\
	scsi-sense-data.h         	\
	scsi-utils.h         		\
	scsi-q-subchannel.h         	\
	scsi-error.c         		\
	scsi-read-track-information.c   \
	scsi-read-track-information.h   \
	scsi-get-performance.c         	\
	scsi-get-performance.h         	\
	scsi-mmc3.h         		\
	scsi-mode-pages.h         	\
	scsi-mode-sense.c         	\
	scsi-status-page.h         	\
	scsi-spc1.h         		\
	scsi-read-capacity.c         	\
	scsi-read-capacity.h         	\
	scsi-read-disc-structure.c      \
	scsi-read-disc-structure.h      \
	scsi-dvd-structures.h         	\
	scsi-read-format-capacities.c   \
	scsi-read-format-capacities.h   \
	scsi-read-cd.h			\
	scsi-read-cd.c			\
	scsi-device.h         		\
	scsi-mech-status.c         	\
	scsi-mech-status.h         	\
	scsi-write-page.h         	\
	scsi-mode-select.c         	\
	scsi-read10.c         		\
	scsi-sbc.h			\
	scsi-test-unit-ready.c          \
	brasero-media.c           	\
	brasero-medium-monitor.c        \
	burn-susp.c         		\
	burn-susp.h         		\
	burn-iso-field.c         	\
	burn-iso-field.h         	\
	burn-iso9660.c         		\
	burn-iso9660.h         		\
	burn-volume-source.c         	\
	burn-volume-source.h         	\
	burn-volume.c         		\
	burn-volume.h         		\
	brasero-medium.c         	\
	brasero-volume.c         	\
	brasero-drive.c         	\
	brasero-medium-selection.c      \
	brasero-units.c         	\
	brasero-drive-selection.c       \
	brasero-drive-selection.h       \
	brasero-media-private.h         \
	brasero-medium-selection-priv.h \
	brasero-gio-operation.h         \
	brasero-gio-operation.c         \
	scsi-inquiry.c         		\
	scsi-prevent-allow-medium-removal.c \
	scsi-inquiry.h         		\
	brasero-drive-priv.h

# FreeBSD's SCSI CAM interface
if HAVE_CAM_LIB_H
libbrasero_media3_la_SOURCES += scsi-cam.c
endif

# Linux's SCSI CAM interface
if HAVE_SG_IO_HDR_T
libbrasero_media3_la_SOURCES += scsi-sg.c
endif

# NetBSD's scsi(4)
if HAVE_SCSIIO_H
libbrasero_media3_la_SOURCES += scsi-netbsd.c
endif

# Solaris's USCSI interface
if HAVE_USCSI_H
libbrasero_media3_la_SOURCES += scsi-uscsi.c
endif

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all

if HAVE_INTROSPECTION
introspected_source = \
	brasero-drive.h			\
	brasero-drive.c			\
	brasero-medium-monitor.h	\
	brasero-medium-monitor.c	\
	brasero-media.h			\
	brasero-media.c			\
	brasero-units.h			\
	brasero-units.c			\
	brasero-medium.h		\
	brasero-medium.c		\
	brasero-volume.h		\
	brasero-volume.c		\
	brasero-medium-selection.h	\
	brasero-medium-selection.c	\
	brasero-drive-selection.h	\
	brasero-drive-selection.c

BraseroMedia-3.@TYPELIB_MIN_VERSION@.gir: Makefile libbrasero-media3.la
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_NAMESPACE = BraseroMedia
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_VERSION = 3.@TYPELIB_MIN_VERSION@
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_INCLUDES = Gtk-3.0
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_CFLAGS = $(AM_CPPFLAGS)
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_LIBS = libbrasero-media3.la
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_FILES = $(introspected_source)
BraseroMedia_3_@TYPELIB_MIN_VERSION@_gir_SCANNERFLAGS = --identifier-prefix=Brasero --symbol-prefix=brasero

INTROSPECTION_GIRS += BraseroMedia-3.@TYPELIB_MIN_VERSION@.gir
endif # HAVE_INTROSPECTION

girdir = $(datadir)/gir-1.0
nodist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
nodist_typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(nodist_gir_DATA) $(nodist_typelib_DATA)

EXTRA_DIST +=			\
	libbrasero-marshal.list
#	libbrasero-media.symbols

CLEANFILES +=			\
	$(RECMARSHALFILES)  \
	$(gir_DATA)         \
	$(typelibs_DATA)

-include $(top_srcdir)/git.mk
