
AM_CPPFLAGS = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GMODULE_EXPORT_CFLAGS)					\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libbrasero-marshal.h libbrasero-marshal.c

libbrasero-marshal.h: libbrasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --header > libbrasero-marshal.h )
libbrasero-marshal.c: libbrasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --body --header > libbrasero-marshal.c )

headerdir=$(includedir)/brasero3
header_HEADERS =	brasero-drive.h			\
		brasero-medium-monitor.h		\
		brasero-media.h			\
		brasero-units.h			\
		brasero-medium.h			\
		brasero-volume.h		\
		brasero-medium-selection.h	\
		brasero-drive-selection.h

lib_LTLIBRARIES = 				\
	libbrasero-media3.la

libbrasero_media3_la_LIBADD =                                     \
	$(BRASERO_GLIB_LIBS)                                     \
	$(BRASERO_GMODULE_EXPORT_LIBS)                                     \
	$(BRASERO_GTHREAD_LIBS)                                     \
	$(BRASERO_GIO_LIBS)                                     \
	$(BRASERO_GTK_LIBS)                                     \
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
	brasero-medium-monitor.h		\
	brasero-media.h			\
	brasero-units.h			\
	brasero-medium.h			\
	brasero-volume.h		\
	brasero-medium-selection.h	\
	scsi-base.h         \
	scsi-command.h         \
	scsi-error.h         \
	scsi-get-configuration.c         \
	scsi-get-configuration.h         \
	scsi-mmc1.h         \
	scsi-mmc2.h         \
	scsi-opcodes.h         \
	scsi-read-disc-info.c         \
	scsi-read-disc-info.h         \
	scsi-read-toc-pma-atip.c         \
	scsi-read-toc-pma-atip.h         \
	scsi-sense-data.c         \
	scsi-sense-data.h         \
	scsi-utils.h         \
	scsi-q-subchannel.h         \
	scsi-error.c         \
	scsi-read-track-information.c         \
	scsi-read-track-information.h         \
	scsi-get-performance.c         \
	scsi-get-performance.h         \
	scsi-mmc3.h         \
	scsi-mode-pages.h         \
	scsi-mode-sense.c         \
	scsi-status-page.h         \
	scsi-spc1.h         \
	scsi-read-capacity.c         \
	scsi-read-capacity.h         \
	scsi-read-disc-structure.c         \
	scsi-read-disc-structure.h         \
	scsi-dvd-structures.h         \
	scsi-read-format-capacities.c         \
	scsi-read-format-capacities.h         \
	scsi-read-cd.h	\
	scsi-read-cd.c	\
	scsi-device.h         \
	scsi-mech-status.c         \
	scsi-mech-status.h         \
	scsi-write-page.h         \
	scsi-mode-select.c         \
	scsi-read10.c         \
	scsi-sbc.h		\
	scsi-test-unit-ready.c           \
	brasero-media.c           \
	brasero-medium-monitor.c         \
	burn-susp.c         \
	burn-susp.h         \
	burn-iso-field.c         \
	burn-iso-field.h         \
	burn-iso9660.c         \
	burn-iso9660.h         \
	burn-volume-source.c         \
	burn-volume-source.h         \
	burn-volume.c         \
	burn-volume.h         \
	brasero-medium.c         \
	brasero-volume.c         \
	brasero-drive.c         \
	brasero-medium-selection.c         \
	brasero-units.c         \
	brasero-drive-selection.c         \
	brasero-drive-selection.h         \
	brasero-media-private.h         \
	brasero-medium-selection-priv.h         \
	brasero-gio-operation.h         \
	brasero-gio-operation.c         \
	scsi-inquiry.c         \
	scsi-prevent-allow-medium-removal.c         \
	scsi-inquiry.h         \
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

if HAVE_INTROSPECTION
girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = BraseroMedia-@TYPELIB_VERSION@.gir

typelibsdir = $(INTROSPECTION_TYPELIBDIR)
typelibs_DATA = BraseroMedia-@TYPELIB_VERSION@.typelib

BraseroMedia-@TYPELIB_VERSION@.gir: libbrasero-media3.la $(INTROSPECTION_SCANNER)
	$(INTROSPECTION_SCANNER) -v --namespace=BraseroMedia --nsversion=1.0 \
            --include=Gtk-3.0 \
	    -I$(top_srcdir)/libbrasero-media	\
	    --library=brasero-media3 \
	    --libtool="$(LIBTOOL)" \
            --output $@ \
            --pkg=gtk+-3.0  \
	    --strip-prefix=Brasero	\
            $(top_srcdir)/libbrasero-media/brasero-drive.h			\
	    $(top_srcdir)/libbrasero-media/brasero-medium-monitor.h		\
	    $(top_builddir)/libbrasero-media/brasero-media.h			\
	    $(top_srcdir)/libbrasero-media/brasero-units.h			\
	    $(top_srcdir)/libbrasero-media/brasero-medium.h			\
	    $(top_srcdir)/libbrasero-media/brasero-volume.h		\
	    $(top_srcdir)/libbrasero-media/brasero-medium-selection.h	\
	    $(top_srcdir)/libbrasero-media/brasero-drive-selection.h	\
            $(top_srcdir)/libbrasero-media/*.c	    

BraseroMedia-@TYPELIB_VERSION@.typelib: $(gir_DATA) $(INTROSPECTION_COMPILER)
	$(INTROSPECTION_COMPILER) $(top_builddir)/libbrasero-media/$(gir_DATA) --shared-library=libbrasero-media3 -o $(typelibs_DATA)
endif

EXTRA_DIST =			\
	libbrasero-marshal.list
#	libbrasero-media.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)  \
	$(gir_DATA)         \
	$(typelibs_DATA)

-include $(top_srcdir)/git.mk
