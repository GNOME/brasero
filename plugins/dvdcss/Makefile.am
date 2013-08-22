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
	$(BRASERO_GLIB_CFLAGS)

plugindir = $(BRASERO_PLUGIN_DIRECTORY)
plugin_LTLIBRARIES = libbrasero-dvdcss.la
libbrasero_dvdcss_la_SOURCES = burn-dvdcss.c \
	burn-dvdcss-private.h 
libbrasero_dvdcss_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GMODULE_LIBS)
libbrasero_dvdcss_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
