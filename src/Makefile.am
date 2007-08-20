SUBDIRS = plugins
DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED \
					-DGDK_DISABLE_DEPRECATED \
					-DGDK_PIXBUF_DISABLE_DEPRECATED \
					-DGTK_DISABLE_DEPRECATED \
					-DGNOME_VFS_DISABLE_DEPRECATED \
					-DGNOME_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I./scsi							\
	-I./md5								\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_CFLAGS)

CLEANFILES = $(RECMARSHALFILES)

AM_CFLAGS = -Wall -g

EXTRA_DIST = cd-content-marshal.list

GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
RECMARSHALFILES = brasero-marshal.h brasero-marshal.c 
brasero-marshal.h: brasero-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/brasero-marshal.list --header > brasero-marshal.h )
brasero-marshal.c: brasero-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=brasero_marshal $(srcdir)/brasero-marshal.list --body --header > brasero-marshal.c )

bin_PROGRAMS = brasero

brasero_SOURCES = \
	$(RECMARSHALFILES)			\
	main.c		\
	brasero-utils.h		\
	brasero-utils.c         \
	brasero-search-entry.c         \
	brasero-search-entry.h         \
	brasero-search.c         \
	brasero-search.h         \
	brasero-mime-filter.c         \
	brasero-mime-filter.h         \
	brasero-playlist.c         \
	brasero-playlist.h         \
	brasero-player.c         \
	brasero-player.h         \
	brasero-menu.h         \
	brasero-burn-dialog.c         \
	brasero-burn-dialog.h         \
	brasero-disc-copy-dialog.c         \
	brasero-disc-copy-dialog.h         \
	brasero-blank-dialog.c         \
	brasero-blank-dialog.h         \
	inotify.h         \
	brasero-metadata.c         \
	brasero-metadata.h         \
	inotify-syscalls.h         \
	brasero-filtered-window.c         \
	brasero-filtered-window.h         \
	brasero-disc.c         \
	brasero-disc.h         \
	brasero-data-disc.c         \
	brasero-data-disc.h         \
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
	brasero-app.h         \
	brasero-uri-container.c         \
	brasero-uri-container.h         \
	brasero-project-manager.c         \
	brasero-project-manager.h         \
	brasero-layout.c         \
	brasero-layout.h         \
	brasero-file-chooser.c         \
	brasero-file-chooser.h         \
	brasero-project-size.c         \
	brasero-project-size.h         \
	burn-session.h 		\
	brasero-sum-dialog.c         \
	brasero-sum-dialog.h         \
	brasero-tool-dialog.c         \
	brasero-tool-dialog.h         \
	brasero-ncb.h         \
	brasero-ncb.c         \
	burn-task.c         \
	burn-task.h         \
	brasero-vfs.c         \
	brasero-vfs.h         \
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
	burn-iso9660.c         \
	burn-iso9660.h         \
	brasero-layout-object.c         \
	brasero-layout-object.h         \
	burn-volume.c         \
	burn-volume.h         \
	burn-susp.c         \
	burn-susp.h         \
	burn-iso-field.c         \
	burn-iso-field.h         \
	burn-medium.h         \
	burn-medium.c         \
	scsi/scsi-base.h         \
	scsi/scsi-command.c         \
	scsi/scsi-command.h         \
	scsi/scsi-error.h         \
	scsi/scsi-get-configuration.c         \
	scsi/scsi-get-configuration.h         \
	scsi/scsi-mmc1.h         \
	scsi/scsi-mmc2.h         \
	scsi/scsi-opcodes.h         \
	scsi/scsi-read-disc-info.c         \
	scsi/scsi-read-disc-info.h         \
	scsi/scsi-read-toc-pma-atip.c         \
	scsi/scsi-read-toc-pma-atip.h         \
	scsi/scsi-sense-data.c         \
	scsi/scsi-sense-data.h         \
	scsi/scsi-sg.c         \
	scsi/scsi-sg.h         \
	scsi/scsi-utils.h         \
	scsi/scsi-q-subchannel.h         \
	scsi/scsi-error.c         \
	scsi/scsi-read-track-information.c         \
	scsi/scsi-read-track-information.h         \
	scsi/scsi-get-performance.c         \
	scsi/scsi-get-performance.h         \
	scsi/scsi-mmc3.h         \
	scsi/scsi-mode-pages.h         \
	scsi/scsi-mode-sense.c         \
	scsi/scsi-status-page.h         \
	scsi/scsi-spc1.h         \
	scsi/scsi-read-capacity.c         \
	scsi/scsi-read-capacity.h         \
	scsi/scsi-read-disc-structure.c         \
	scsi/scsi-read-disc-structure.h         \
	scsi/scsi-dvd-structures.h         \
	scsi/scsi-read-format-capacities.c         \
	scsi/scsi-read-format-capacities.h         \
	burn-debug.c         \
	burn-debug.h         \
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
	brasero-drive-selection.h         \
	brasero-drive-selection.c         \
	brasero-src-selection.h         \
	brasero-src-selection.c         \
	brasero-dest-selection.h         \
	brasero-dest-selection.c         \
	brasero-drive-info.c         \
	brasero-drive-info.h         \
	brasero-drive-properties.h         \
	brasero-drive-properties.c         \
	brasero-image-properties.h         \
	brasero-image-properties.c         \
	brasero-xfer.c         \
	brasero-xfer.h         \
	burn-session.c         \
	burn-track.c 

brasero_LDADD = $(BRASERO_LIBS)
































































































