plugindir = $(libdir)/brasero/plugins

DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/src					\
	-I$(top_srcdir)/src/md5/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_BASE_CFLAGS) 			\
	$(BRASERO_GNOMEVFS_CFLAGS)

AM_CFLAGS = -g

#local-track
plugin_LTLIBRARIES = libbrasero-local-track.la

libbrasero_local_track_la_SOURCES = burn-local-image.c burn-local-image.h
libbrasero_local_track_la_LIBADD = $(BRASERO_BASE_LIBS) $(BRASERO_GNOMEVFS_LIBS)
libbrasero_local_track_la_LDFLAGS = -module -avoid-version
