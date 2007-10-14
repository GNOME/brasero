
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

#cdrdao
cdrdaodir = $(libdir)/brasero/plugins
cdrdao_LTLIBRARIES = libbrasero-cdrdao.la
libbrasero_cdrdao_la_SOURCES = burn-cdrdao.c burn-cdrdao.h
libbrasero_cdrdao_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_cdrdao_la_LDFLAGS = -module -avoid-version

#toc2cue
toc2cuedir = $(libdir)/brasero/plugins
toc2cue_LTLIBRARIES = libbrasero-toc2cue.la
libbrasero_toc2cue_la_SOURCES = burn-toc2cue.c burn-toc2cue.h
libbrasero_toc2cue_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_toc2cue_la_LDFLAGS = -module -avoid-version
