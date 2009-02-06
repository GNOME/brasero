SUBDIRS = plugins

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libbrasero-media/					\
	-I$(top_builddir)/libbrasero-media/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_GCONF_CFLAGS)						\
	$(BRASERO_GSTREAMER_CFLAGS)					\
	$(BRASERO_GSTREAMER_BASE_CFLAGS)				\
	$(BRASERO_SM_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_DBUS_CFLAGS)						\
	$(BRASERO_GMODULE_CFLAGS)					\
	$(BRASERO_LIBXML_CFLAGS)					\
	$(BRASERO_BEAGLE_CFLAGS)					\
	$(BRASERO_PL_PARSER_CFLAGS)

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
	burn-dbus.h	\
	burn-dbus.c	\
	brasero-search-entry.c         \
	brasero-search-entry.h         \
	brasero-search-beagle.c         \
	brasero-search-beagle.h         \
	brasero-mime-filter.c         \
	brasero-mime-filter.h         \
	brasero-playlist.c         \
	brasero-playlist.h         \
	brasero-player.c         \
	brasero-player.h         \
	brasero-burn-dialog.c         \
	brasero-burn-dialog.h         \
	brasero-disc-copy-dialog.c         \
	brasero-disc-copy-dialog.h         \
	brasero-blank-dialog.c         \
	brasero-blank-dialog.h         \
	brasero-eject-dialog.c		\
	brasero-eject-dialog.h		\
	brasero-metadata.c         \
	brasero-metadata.h         \
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
	brasero-progress.c         \
	brasero-progress.h         \
	brasero-song-properties.c         \
	brasero-song-properties.h         \
	brasero-tray.c         \
	brasero-tray.h         \
	burn.c         \
	burn.h         \
	burn-job.c         \
	burn-job.h         \
	burn-process.c         \
	burn-process.h         \
	burn-basics.h         \
	burn-basics.c         \
	burn-caps.c         \
	burn-caps.h         \
	brasero-session.c         \
	brasero-session.h         \
	brasero-uri-container.c         \
	brasero-uri-container.h         \
	brasero-project-manager.c         \
	brasero-project-manager.h         \
	brasero-layout.c         \
	brasero-layout.h         \
	brasero-file-chooser.c         \
	brasero-file-chooser.h         \
	burn-session.h 		\
	brasero-sum-dialog.c         \
	brasero-sum-dialog.h         \
	brasero-tool-dialog.c         \
	brasero-tool-dialog.h         \
	burn-task.c         \
	burn-task.h         \
	brasero-async-task-manager.c         \
	brasero-async-task-manager.h         \
	eggtreemultidnd.c         \
	eggtreemultidnd.h         \
	brasero-multi-dnd.c         \
	brasero-multi-dnd.h         \
	brasero-image-option-dialog.c         \
	brasero-image-option-dialog.h         \
	brasero-disc-option-dialog.c         \
	brasero-disc-option-dialog.h         \
	brasero-image-type-chooser.c         \
	brasero-image-type-chooser.h         \
	brasero-layout-object.c         \
	brasero-layout-object.h         \
	burn-track.h         \
	burn-plugin.h         \
	burn-plugin.c         \
	burn-plugin-private.h         \
	burn-plugin-manager.c         \
	burn-plugin-manager.h         \
	burn-task-ctx.h         \
	burn-task-item.c         \
	burn-task-item.h         \
	burn-task-ctx.c         \
	burn-mkisofs-base.c         \
	burn-mkisofs-base.h         \
	brasero-src-selection.h         \
	brasero-src-selection.c         \
	brasero-dest-selection.h         \
	brasero-dest-selection.c         \
	brasero-drive-properties.h         \
	brasero-drive-properties.c         \
	brasero-image-properties.h         \
	brasero-image-properties.c         \
	brasero-xfer.c         \
	brasero-xfer.h         \
	burn-session.c         \
	burn-track.c         \
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
	burn-image-format.c         \
	burn-image-format.h         \
	brasero-data-project.c         \
	brasero-data-project.h         \
	brasero-data-session.c         \
	brasero-data-session.h         \
	brasero-data-tree-model.c         \
	brasero-data-tree-model.h         \
	brasero-data-vfs.c         \
	brasero-data-vfs.h         \
	brasero-file-node.c         \
	brasero-file-node.h         \
	brasero-data-disc.h         \
	brasero-data-disc.c         \
	brasero-file-filtered.h         \
	brasero-file-filtered.c         \
	brasero-filter-option.h         \
	brasero-filter-option.c         \
	brasero-disc-message.h         \
	brasero-disc-message.c         \
	baobab-cell-renderer-progress.c         \
	baobab-cell-renderer-progress.h         \
	brasero-io.h         \
	brasero-io.c         \
	brasero-jacket-background.c         \
	brasero-jacket-background.h         \
	brasero-jacket-buffer.c         \
	brasero-jacket-buffer.h         \
	brasero-jacket-edit.c         \
	brasero-jacket-edit.h         \
	brasero-jacket-font.c         \
	brasero-jacket-font.h         \
	brasero-jacket-view.c         \
	brasero-jacket-view.h         \
	brasero-multi-song-props.h         \
	brasero-multi-song-props.c         \
	brasero-rename.h         \
	brasero-rename.c         \
	brasero-notify.c         \
	brasero-notify.h         \
	brasero-video-disc.c         \
	brasero-video-disc.h         \
	brasero-video-project.h         \
	brasero-video-project.c         \
	brasero-video-tree-model.c         \
	brasero-video-tree-model.h         \
	brasero-app.c		\
	brasero-app.h           \
	brasero-session-cfg.h           \
	brasero-session-cfg.c           \
	brasero-burn-options.h           \
	brasero-burn-options.c           \
	brasero-project-name.c           \
	brasero-project-name.h           \
	brasero-medium-properties.h           \
	brasero-medium-properties.c           \
	brasero-src-image.h           \
	brasero-src-image.c           \
	brasero-tool-color-picker.h           \
	brasero-tool-color-picker.c           \
	burn-debug.c           \
	burn-debug.h           \
	eggdesktopfile.c           \
	eggdesktopfile.h           \
	eggsmclient.c           \
	eggsmclient.h           \
	eggsmclient-private.h           \
	eggsmclient-xsmp.c           \
	brasero-project-parse.c           \
	brasero-project-parse.h

if BUILD_INOTIFY
brasero_SOURCES += brasero-file-monitor.c brasero-file-monitor.h
endif

brasero_LDADD =				\
	../libbrasero-media/libbrasero-media.la		\
	$(BRASERO_GLIB_LIBS)		\
	$(BRASERO_GIO_LIBS)		\
	$(BRASERO_GCONF_LIBS)		\
	$(BRASERO_GSTREAMER_LIBS)	\
	$(BRASERO_GSTREAMER_BASE_LIBS)	\
	$(BRASERO_SM_LIBS)		\
	$(BRASERO_GTK_LIBS)		\
	$(BRASERO_GMODULE_LIBS)		\
	$(BRASERO_DBUS_LIBS)		\
	$(BRASERO_LIBXML_LIBS)		\
	$(BRASERO_BEAGLE_LIBS)		\
	$(BRASERO_PL_PARSER_LIBS)

EXTRA_DIST =			\
	brasero-marshal.list

CLEANFILES =			\
	$(RECMARSHALFILES)
	
