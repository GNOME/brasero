SUBDIRS = plugins

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/libbrasero-utils/				\
	-I$(top_builddir)/libbrasero-utils/				\
	-I$(top_srcdir)/libbrasero-media/				\
	-I$(top_builddir)/libbrasero-media/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GMODULE_CFLAGS)					\
	$(BRASERO_GCONF_CFLAGS)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_DBUS_CFLAGS)

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = libbrasero-marshal.h libbrasero-marshal.c

libbrasero-marshal.h: libbrasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --header > libbrasero-marshal.h )
libbrasero-marshal.c: libbrasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/libbrasero-marshal.list --body --header > libbrasero-marshal.c )

lib_LTLIBRARIES = \
	libbrasero-burn.la

libbraseroburnincludedir = $(pkgincludedir)/
libbraseroburninclude_HEADERS =					\
	brasero-tags.h						\
	brasero-enums.h						\
	brasero-error.h						\
	brasero-burn-lib.h					\
	brasero-track.h						\
	brasero-track-data.h                 			\
	brasero-track-disc.h                 			\
	brasero-track-stream.h                 			\
	brasero-track-image.h					\
	brasero-track-image-cfg.h				\
	brasero-session.h                 			\
	brasero-burn.h						\
	brasero-session-cfg.h					\
	brasero-burn-options.h        			        \
	brasero-burn-dialog.h

libbrasero_burn_la_LIBADD =					\
	../libbrasero-media/libbrasero-media.la			\
	../libbrasero-utils/libbrasero-utils.la			\
	$(BRASERO_GLIB_LIBS)					\
	$(BRASERO_GMODULE_LIBS)					\
	$(BRASERO_GIO_LIBS)					\
	$(BRASERO_GCONF_LIBS)					\
	$(BRASERO_GTK_LIBS)					\
	$(BRASERO_DBUS_LIBS)

libbrasero_burn_la_LDFLAGS =					\
	-version-info $(LIBBRASERO_LT_VERSION)			\
	-no-undefined						\
	$(AM_LDFLAGS)
#	-export-symbols ./libbrasero-burn.symbols

libbrasero_burn_la_SOURCES = 		\
	libbrasero-marshal.c		\
	libbrasero-marshal.h		\
	brasero-burn.c			\
	brasero-burn.h			\
	burn-basics.h                 \
	burn-caps.h                 \
	burn-dbus.h                 \
	burn-debug.h                 \
	burn-image-format.h                 \
	burn-job.h                 \
	burn-mkisofs-base.h                 \
	burn-plugin.h                 \
	burn-plugin-manager.h                 \
	burn-plugin-private.h                 \
	burn-process.h                 \
	brasero-session.h                 \
	burn-task.h                 \
	burn-task-ctx.h                 \
	burn-task-item.h                 \
	brasero-track.h                 \
	brasero-session.c                 \
	brasero-track.c                 \
	burn-basics.c                 \
	burn-caps.c                 \
	burn-dbus.c                 \
	burn-debug.c                 \
	burn-image-format.c                 \
	burn-job.c                 \
	burn-mkisofs-base.c                 \
	burn-plugin.c                 \
	burn-plugin-manager.c                 \
	burn-process.c                 \
	burn-task.c                 \
	burn-task-ctx.c                 \
	burn-task-item.c                 \
	brasero-burn-dialog.c                 \
	brasero-burn-dialog.h                 \
	brasero-burn-options.c                 \
	brasero-burn-options.h                 \
	brasero-dest-selection.c                 \
	brasero-dest-selection.h                 \
	brasero-disc-copy-dialog.c                 \
	brasero-disc-copy-dialog.h                 \
	brasero-disc-option-dialog.c                 \
	brasero-disc-option-dialog.h                 \
	brasero-drive-properties.c                 \
	brasero-drive-properties.h                 \
	brasero-image-option-dialog.c                 \
	brasero-image-option-dialog.h                 \
	brasero-image-properties.c                 \
	brasero-image-properties.h                 \
	brasero-image-type-chooser.c                 \
	brasero-image-type-chooser.h                 \
	brasero-medium-properties.c                 \
	brasero-medium-properties.h                 \
	brasero-progress.c                 \
	brasero-progress.h                 \
	brasero-session-cfg.c                 \
	brasero-session-cfg.h                 \
	brasero-src-image.c                 \
	brasero-src-image.h                 \
	brasero-src-selection.c                 \
	brasero-src-selection.h                 \
	brasero-tray.c                 \
	brasero-tray.h                 \
	brasero-burn-lib.h                 \
	brasero-error.h                 \
	brasero-enums.h                 \
	brasero-tags.h                 \
	brasero-cover.c                 \
	brasero-cover.h                 \
	brasero-track-disc.c                 \
	brasero-track-disc.h                 \
	brasero-track-stream.c                 \
	brasero-track-stream.h                 \
	brasero-track-image.c                 \
	brasero-track-image.h                 \
	brasero-track-data.c                 \
	brasero-track-data.h                 \
	brasero-track-image-cfg.c                 \
	brasero-track-image-cfg.h                 \
	brasero-burn-options-private.h                 \
	brasero-caps-plugin.c                 \
	brasero-caps-burn.c                 \
	brasero-caps-session.c                 \
	brasero-caps-burn.h

EXTRA_DIST =			\
	libbrasero-marshal.list
#	libbrasero-burn.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)

