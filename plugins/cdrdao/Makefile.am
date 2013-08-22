
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

#cdrdao
cdrdaodir = $(BRASERO_PLUGIN_DIRECTORY)
cdrdao_LTLIBRARIES = libbrasero-cdrdao.la
libbrasero_cdrdao_la_SOURCES = burn-cdrdao.c 
libbrasero_cdrdao_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_cdrdao_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
