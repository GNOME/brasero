plugindir = $(libdir)/brasero/plugins

INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/brasero-media/					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/src				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_GLIB_CFLAGS)

plugin_LTLIBRARIES = libbrasero-dvdcss.la

libbrasero_dvdcss_la_SOURCES = burn-dvdcss.c burn-dvdcss.h \
	burn-dvdcss-private.h 
libbrasero_dvdcss_la_LIBADD = $(BRASERO_GLIB_LIBS)
libbrasero_dvdcss_la_LDFLAGS = -module -avoid-version
