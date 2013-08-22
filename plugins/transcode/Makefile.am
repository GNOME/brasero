
AM_CPPFLAGS = \
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
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_GLIB_CFLAGS)				\
	$(BRASERO_GSTREAMER_CFLAGS)

transcodedir = $(BRASERO_PLUGIN_DIRECTORY)
transcode_LTLIBRARIES = libbrasero-transcode.la

libbrasero_transcode_la_SOURCES = burn-transcode.c burn-normalize.h 
libbrasero_transcode_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS)
libbrasero_transcode_la_LDFLAGS = -module -avoid-version

normalizedir = $(BRASERO_PLUGIN_DIRECTORY)
normalize_LTLIBRARIES = libbrasero-normalize.la

libbrasero_normalize_la_SOURCES = burn-normalize.c burn-normalize.h
libbrasero_normalize_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS)
libbrasero_normalize_la_LDFLAGS = -module -avoid-version

vobdir = $(BRASERO_PLUGIN_DIRECTORY)
vob_LTLIBRARIES = libbrasero-vob.la

libbrasero_vob_la_SOURCES = burn-vob.c 
libbrasero_vob_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GSTREAMER_LIBS)
libbrasero_vob_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
