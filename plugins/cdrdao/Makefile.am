
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
	$(BRASERO_GCONF_CFLAGS)

#cdrdao
cdrdaodir = $(libdir)/brasero/plugins
cdrdao_LTLIBRARIES = libbrasero-cdrdao.la
libbrasero_cdrdao_la_SOURCES = burn-cdrdao.c burn-cdrdao.h \
	burn-cdrdao-common.h 
libbrasero_cdrdao_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GCONF_LIBS) ../../libbrasero-media/libbrasero-media.la ../../libbrasero-burn/libbrasero-burn.la
libbrasero_cdrdao_la_LDFLAGS = -module -avoid-version

#toc2cue
toc2cuedir = $(libdir)/brasero/plugins
toc2cue_LTLIBRARIES = libbrasero-toc2cue.la
libbrasero_toc2cue_la_SOURCES = burn-toc2cue.c burn-toc2cue.h \
	burn-cdrdao-common.h 
libbrasero_toc2cue_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_toc2cue_la_LDFLAGS = -module -avoid-version
