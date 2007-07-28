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
	$(BRASERO_BASE_CFLAGS)

AM_CFLAGS = -Wall -g

plugin_LTLIBRARIES = libbrasero-dvdcss.la

libbrasero_dvdcss_la_SOURCES = burn-dvdcss.c burn-dvdcss.h
libbrasero_dvdcss_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_dvdcss_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)
