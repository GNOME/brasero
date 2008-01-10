plugindir = $(libdir)/brasero/plugins

DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED \
		     -DGDK_DISABLE_DEPRECATED \
		     -DGDK_PIXBUF_DISABLE_DEPRECATED \
		     -DGTK_DISABLE_DEPRECATED \
		     -DGNOME_VFS_DISABLE_DEPRECATED \
		     -DGNOME_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/src				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_BASE_CFLAGS)				\
	$(BRASERO_GSTREAMER_CFLAGS)

AM_CFLAGS = -g

plugin_LTLIBRARIES = libbrasero-transcode.la

libbrasero_transcode_la_SOURCES = burn-transcode.c burn-transcode.h
libbrasero_transcode_la_LIBADD = $(BRASERO_BASE_LIBS) $(BRASERO_GSTREAMER_CFLAGS)
libbrasero_transcode_la_LDFLAGS = -module -avoid-version
