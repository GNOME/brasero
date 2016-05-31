
AM_CPPFLAGS = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libbrasero-utils/				\
	-I$(top_builddir)/libbrasero-utils/				\
	-I$(top_srcdir)/libbrasero-media/				\
	-I$(top_builddir)/libbrasero-media/				\
	-I$(top_srcdir)/libbrasero-burn/				\
	-I$(top_builddir)/libbrasero-burn/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	-DEGG_SM_CLIENT_BACKEND_XSMP					\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GSTREAMER_CFLAGS)					\
	$(BRASERO_GSTREAMER_BASE_CFLAGS)				\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_DBUS_CFLAGS)						\
	$(BRASERO_LIBXML_CFLAGS)					\
	$(BRASERO_SEARCH_CFLAGS)					\
	$(BRASERO_PL_PARSER_CFLAGS)					\
	$(BRASERO_SM_CFLAGS) 						\
	-Wno-error=format-y2k

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = brasero-marshal.h brasero-marshal.c

brasero-marshal.h: brasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/brasero-marshal.list --header > brasero-marshal.h )
brasero-marshal.c: brasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/brasero-marshal.list --body --header > brasero-marshal.c )

bin_PROGRAMS = brasero

brasero_SOURCES = \
	brasero-marshal.c	\
	brasero-marshal.h	\
	main.c		\
	brasero-utils.h		\
	brasero-utils.c         \
	brasero-player.c         \
	brasero-player.h         \
	brasero-eject-dialog.c		\
	brasero-eject-dialog.h		\
	brasero-disc.c         \
	brasero-disc.h         \
	brasero-audio-disc.c         \
	brasero-audio-disc.h         \
	brasero-project.c         \
	brasero-project.h         \
	brasero-project-type-chooser.c         \
	brasero-project-type-chooser.h         \
	brasero-player-bacon.c         \
	brasero-player-bacon.h         \
	brasero-song-properties.c         \
	brasero-song-properties.h         \
	brasero-uri-container.c         \
	brasero-uri-container.h         \
	brasero-project-manager.c         \
	brasero-project-manager.h         \
	brasero-layout.c         \
	brasero-layout.h         \
	brasero-file-chooser.c         \
	brasero-file-chooser.h         \
	eggtreemultidnd.c         \
	eggtreemultidnd.h         \
	brasero-multi-dnd.c         \
	brasero-multi-dnd.h         \
	brasero-layout-object.c         \
	brasero-layout-object.h         \
	brasero-plugin-manager-ui.c         \
	brasero-plugin-manager-ui.h         \
	brasero-pref.h         \
	brasero-pref.c         \
	brasero-plugin-option.h         \
	brasero-plugin-option.c         \
	brasero-split-dialog.h         \
	brasero-split-dialog.c         \
	brasero-time-button.h         \
	brasero-time-button.c         \
	brasero-preview.h         \
	brasero-preview.c         \
	brasero-data-disc.h         \
	brasero-data-disc.c         \
	brasero-file-filtered.h         \
	brasero-file-filtered.c         \
	brasero-filter-option.h         \
	brasero-filter-option.c         \
	baobab-cell-renderer-progress.c         \
	baobab-cell-renderer-progress.h         \
	brasero-multi-song-props.h         \
	brasero-multi-song-props.c         \
	brasero-rename.h         \
	brasero-rename.c         \
	brasero-video-disc.c         \
	brasero-video-disc.h         \
	brasero-video-tree-model.c         \
	brasero-video-tree-model.h         \
	brasero-app.c		\
	brasero-app.h           \
	brasero-project-name.c           \
	brasero-project-name.h           \
	brasero-project-parse.c           \
	brasero-project-parse.h           \
	brasero-setting.h        \
	brasero-setting.c        \
	brasero-search-engine.h        \
	brasero-search-engine.c        \
	brasero-drive-settings.h        \
	brasero-drive-settings.c	\
	brasero-song-control.h        \
	brasero-song-control.c        \
	brasero-cli.c        \
	brasero-cli.h

if BUILD_PLAYLIST
brasero_SOURCES += \
	brasero-playlist.c         \
	brasero-playlist.h

endif

if BUILD_TRACKER
brasero_SOURCES += \
		   brasero-search-tracker.h	\
		   brasero-search-tracker.c
endif

brasero_LDADD =						\
	$(top_builddir)/libbrasero-media/libbrasero-media3.la	\
	$(top_builddir)/libbrasero-burn/libbrasero-burn3.la	\
	$(top_builddir)/libbrasero-utils/libbrasero-utils3.la	\
	$(BRASERO_GLIB_LIBS)		\
	$(BRASERO_GTHREAD_LIBS)				\
	$(BRASERO_GIO_LIBS)		\
	$(BRASERO_GSTREAMER_LIBS)	\
	$(BRASERO_GSTREAMER_BASE_LIBS)	\
	$(BRASERO_GTK_LIBS)		\
	$(BRASERO_GMODULE_LIBS)		\
	$(BRASERO_DBUS_LIBS)		\
	$(BRASERO_LIBXML_LIBS)		\
	$(BRASERO_SEARCH_LIBS)		\
	$(BRASERO_PL_PARSER_LIBS)	\
	$(BRASERO_SM_LIBS)

EXTRA_DIST =			\
	brasero-marshal.list

CLEANFILES =			\
	$(RECMARSHALFILES)

-include $(top_srcdir)/git.mk
