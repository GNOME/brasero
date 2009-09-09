
INCLUDES = \
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
local_trackdir = $(libdir)/brasero/plugins
local_track_LTLIBRARIES = libbrasero-local-track.la

libbrasero_local_track_la_SOURCES = burn-local-image.c burn-local-image.h
libbrasero_local_track_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_local_track_la_LDFLAGS = -module -avoid-version

#burn-uri
burn_uridir = $(libdir)/brasero/plugins
burn_uri_LTLIBRARIES = libbrasero-burn-uri.la

libbrasero_burn_uri_la_SOURCES = burn-uri.c burn-uri.h
libbrasero_burn_uri_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GIO_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_burn_uri_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
