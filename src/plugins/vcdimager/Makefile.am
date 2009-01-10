
INCLUDES = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libbrasero-media/					\
	-I$(top_builddir)				\
	-I$(top_srcdir)/src				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)					\
	$(BRASERO_GLIB_CFLAGS)					\
	$(BRASERO_LIBXML_CFLAGS)

vcdimagerdir = $(libdir)/brasero/plugins
vcdimager_LTLIBRARIES = libbrasero-vcdimager.la

libbrasero_vcdimager_la_SOURCES = burn-vcdimager.c burn-vcdimager.h
libbrasero_vcdimager_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_LIBXML_LIBS)
libbrasero_vcdimager_la_LDFLAGS = -module -avoid-version

