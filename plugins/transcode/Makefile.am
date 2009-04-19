
INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libbrasero-media/					\
	-I$(top_builddir)/libbrasero-media/		\
	-I$(top_srcdir)/libbrasero-burn				\
	-I$(top_builddir)/libbrasero-burn/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_GLIB_CFLAGS)				\
	$(BRASERO_GSTREAMER_CFLAGS)

transcodedir = $(libdir)/brasero/plugins
transcode_LTLIBRARIES = libbrasero-transcode.la

libbrasero_transcode_la_SOURCES = burn-transcode.c burn-transcode.h burn-normalize.h
libbrasero_transcode_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_transcode_la_LDFLAGS = -module -avoid-version

normalizedir = $(libdir)/brasero/plugins
normalize_LTLIBRARIES = libbrasero-normalize.la

libbrasero_normalize_la_SOURCES = burn-normalize.c burn-normalize.h
libbrasero_normalize_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_normalize_la_LDFLAGS = -module -avoid-version

vobdir = $(libdir)/brasero/plugins
vob_LTLIBRARIES = libbrasero-vob.la

libbrasero_vob_la_SOURCES = burn-vob.c burn-vob.h
libbrasero_vob_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_vob_la_LDFLAGS = -module -avoid-version
