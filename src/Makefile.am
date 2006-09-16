
DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED \
					-DGDK_DISABLE_DEPRECATED \
					-DGDK_PIXBUF_DISABLE_DEPRECATED \
					-DGTK_DISABLE_DEPRECATED \
					-DGNOME_VFS_DISABLE_DEPRECATED \
					-DGNOME_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_CFLAGS)

CLEANFILES = $(RECMARSHALFILES)

AM_CFLAGS =\
		 -Wall\
		 -g

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
	utils.h		\
	utils.c         \
	search-entry.c         \
	search-entry.h         \
	search.c         \
	search.h         \
	mime-filter.c         \
	mime-filter.h         \
	play-list.c         \
	play-list.h         \
	player.c         \
	player.h         \
	menu.h         \
	recorder-selection.c         \
	recorder-selection.h         \
	burn-dialog.c         \
	burn-dialog.h         \
	disc-copy-dialog.c         \
	disc-copy-dialog.h         \
	blank-dialog.c         \
	blank-dialog.h         \
	inotify.h         \
	metadata.c         \
	metadata.h         \
	inotify-syscalls.h         \
	filtered-window.c         \
	filtered-window.h         \
	disc.c         \
	disc.h         \
	data-disc.c         \
	data-disc.h         \
	audio-disc.c         \
	audio-disc.h         \
	project.c         \
	project.h         \
	project-type-chooser.c         \
	project-type-chooser.h         \
	player-bacon.c         \
	player-bacon.h         \
	progress.c         \
	progress.h         \
	burn-options-dialog.c         \
	burn-options-dialog.h         \
	async-job-manager.c         \
	async-job-manager.h         \
	song-properties.c         \
	song-properties.h         \
	eggtrayicon.c         \
	eggtrayicon.h         \
	tray.c         \
	tray.h         \
	burn.c         \
	burn.h         \
	burn-job.c         \
	burn-job.h         \
	burn-process.c         \
	burn-process.h         \
	burn-recorder.c         \
	burn-recorder.h         \
	burn-imager.c         \
	burn-imager.h         \
	burn-mkisofs.c         \
	burn-mkisofs.h         \
	burn-cdrecord.c         \
	burn-cdrecord.h         \
	burn-cdrdao.c         \
	burn-cdrdao.h         \
	burn-growisofs.c         \
	burn-growisofs.h         \
	burn-readcd.c         \
	burn-readcd.h         \
	burn-mkisofs-base.c         \
	burn-mkisofs-base.h         \
	burn-dvd-rw-format.c         \
	burn-dvd-rw-format.h         \
	burn-transcode.c         \
	burn-transcode.h         \
	burn-basics.h         \
	burn-basics.c         \
	burn-common.c         \
	burn-common.h         \
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
	burn-local-image.c         \
	burn-local-image.h         \
	brasero-project-size.c         \
	brasero-project-size.h         \
	burn-sum.c         \
	burn-sum.h         \
	burn-session.c         \
	burn-session.h 		\
	burn-libburn.c         \
	burn-libburn.h         \
	burn-libisofs.c         \
	burn-libisofs.h         \
	burn-libread-disc.c         \
	burn-libread-disc.h         \
	burn-libburn-common.c         \
	burn-libburn-common.h         \
	brasero-sum-dialog.c         \
	brasero-sum-dialog.h         \
	brasero-tool-dialog.c         \
	brasero-tool-dialog.h         \
	brasero-ncb.h         \
	brasero-ncb.c         \
	burn-md5.c         \
	burn-md5.h         \
	burn-xfer.c         \
	burn-xfer.h         \
	brasero-sum-check.c         \
	brasero-sum-check.h         \
	burn-task.c         \
	burn-task.h 

brasero_LDFLAGS =

brasero_LDADD = $(BRASERO_LIBS)


