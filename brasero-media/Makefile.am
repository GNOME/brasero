
INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/brasero-media/					\
	-I$(top_srcdir)/src/					\
	-I$(top_builddir)						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_DBUS_CFLAGS)						\
	$(BRASERO_HAL_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libbrasero-marshal.h libbrasero-marshal.c

libbrasero-marshal.h: libbrasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --header > libbrasero-marshal.h )
libbrasero-marshal.c: libbrasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --body --header > libbrasero-marshal.c )

lib_LTLIBRARIES = \
	libbrasero-media.la

libbraseromediaincludedir = $(pkgincludedir)/
libbraseromediainclude_HEADERS =	\
	burn-drive.h			\
	burn-medium-monitor.h		\
	burn-media.h			\
	burn-units.h			\
	burn-medium.h			\
	burn-volume-obj.h		\
	brasero-medium-selection.h	\
	brasero-drive-selection.h

libbrasero_media_la_LIBADD =					\
	$(BRASERO_GLIB_LIBS)					\
	$(BRASERO_GIO_LIBS)					\
	$(BRASERO_GTK_LIBS)					\
	$(BRASERO_DBUS_LIBS)					\
	$(BRASERO_HAL_LIBS)

libbrasero_media_la_LDFLAGS =					\
	-version-info $(LIBBRASERO_LT_VERSION)			\
	-no-undefined						\
	$(AM_LDFLAGS)
#	-export-symbols $(srcdir)/libbrasero-media.symbols

libbrasero_media_la_SOURCES =		\
	libbrasero-marshal.c		\
	libbrasero-marshal.h		\
	burn-drive.h			\
	burn-medium-monitor.h		\
	burn-media.h			\
	burn-units.h			\
	burn-medium.h			\
	burn-volume-obj.h		\
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
	scsi-eject.c         \
	scsi-mech-status.c         \
	scsi-mech-status.h         \
	scsi-write-page.h         \
	scsi-mode-select.c         \
	scsi-read10.c         \
	scsi-sbc.h		\
	scsi-test-unit-ready.c           \
	burn-media.c           \
	burn-hal-watch.c           \
	burn-hal-watch.h           \
	burn-medium-monitor.c         \
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
	burn-medium.c         \
	burn-volume-obj.c         \
	burn-drive.c         \
	brasero-medium-selection.c         \
	burn-units.c         \
	brasero-drive-selection.c         \
	brasero-drive-selection.h

# FreeBSD's SCSI CAM interface
if HAVE_CAM_LIB_H
libbrasero_media_la_SOURCES += scsi-cam.c
endif

# Linux's SCSI CAM interface
if HAVE_SG_IO_HDR_T
libbrasero_media_la_SOURCES += scsi-sg.c
endif

# Solaris's USCSI interface
if HAVE_USCSI_H
libbrasero_media_la_SOURCES += scsi-uscsi.c
endif

EXTRA_DIST =			\
	libbrasero-marshal.list	\
	libbrasero-media.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)
	
