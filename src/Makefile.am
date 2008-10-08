SUBDIRS = plugins
DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED \
					-DGDK_DISABLE_DEPRECATED \
					-DGDK_PIXBUF_DISABLE_DEPRECATED \
					-DGTK_DISABLE_DEPRECATED \
					-DGNOME_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_CFLAGS)						\
	$(BRASERO_LIBXML_CFLAGS)

CLEANFILES = $(RECMARSHALFILES)

AM_CFLAGS = -g

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
	scsi-device.h         \
	brasero-io.h         \
	brasero-io.c         \
	brasero-medium-selection.h         \
	brasero-medium-selection.c         \
	burn-medium-monitor.c         \
	burn-medium-monitor.h         \
	burn-drive.h         \
	burn-drive.c         \
	burn-volume-obj.c         \
	burn-volume-obj.h         \
	scsi-eject.c         \
	scsi-mech-status.c         \
	scsi-mech-status.h         \
	burn-volume-read.h         \
	burn-volume-read.c         \
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
	burn-volume-source.c         \
	burn-volume-source.h         \
	brasero-video-disc.c         \
	brasero-video-disc.h         \
	brasero-video-project.h         \
	brasero-video-project.c         \
	brasero-video-tree-model.c         \
	brasero-video-tree-model.h         \
	scsi-write-page.h         \
	scsi-mode-select.c         \
	scsi-read10.c         \
	scsi-sbc.h		\
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
	burn-media.c           \
	burn-media.h

if BUILD_INOTIFY
brasero_SOURCES += brasero-file-monitor.c brasero-file-monitor.h
endif

# FreeBSD's SCSI CAM interface
if HAVE_CAM_LIB_H
brasero_SOURCES += scsi-cam.c
endif

# Linux's SCSI CAM interface
if HAVE_SG_IO_HDR_T
brasero_SOURCES += scsi-sg.c
endif

# Solaris's USCSI interface
if HAVE_USCSI_H
brasero_SOURCES += scsi-uscsi.c
endif

brasero_LDADD =	\
	$(BRASERO_LIBS) $(BRASERO_LIBXML_LIBS)

