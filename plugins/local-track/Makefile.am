
AM_CPPFLAGS = \
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libbrasero-media/					\
	-I$(top_builddir)/libbrasero-media/			\
	-I$(top_srcdir)/libbrasero-burn				\
	-I$(top_builddir)/libbrasero-burn/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)					\
	$(BRASERO_GLIB_CFLAGS)					\
	$(BRASERO_GIO_CFLAGS)

#local-track
local_trackdir = $(BRASERO_PLUGIN_DIRECTORY)
local_track_LTLIBRARIES = libbrasero-local-track.la

libbrasero_local_track_la_SOURCES = burn-local-image.c 
libbrasero_local_track_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS)
libbrasero_local_track_la_LDFLAGS = -module -avoid-version

#burn-uri
burn_uridir = $(BRASERO_PLUGIN_DIRECTORY)
burn_uri_LTLIBRARIES = libbrasero-burn-uri.la

libbrasero_burn_uri_la_SOURCES = burn-uri.c 
libbrasero_burn_uri_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS)
libbrasero_burn_uri_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
