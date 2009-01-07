
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
	$(DISABLE_DEPRECATED)					\
	$(BRASERO_GLIB_CFLAGS)					\
	$(BRASERO_GIO_CFLAGS)

#local-track
local_trackdir = $(libdir)/brasero/plugins
local_track_LTLIBRARIES = libbrasero-local-track.la

libbrasero_local_track_la_SOURCES = burn-local-image.c burn-local-image.h
libbrasero_local_track_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS)
libbrasero_local_track_la_LDFLAGS = -module -avoid-version

#burn-uri
burn_uridir = $(libdir)/brasero/plugins
burn_uri_LTLIBRARIES = libbrasero-burn-uri.la

libbrasero_burn_uri_la_SOURCES = burn-uri.c burn-uri.h
libbrasero_burn_uri_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS)
libbrasero_burn_uri_la_LDFLAGS = -module -avoid-version
