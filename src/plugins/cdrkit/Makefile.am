
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

#woodim
woodimdir = $(libdir)/brasero/plugins
woodim_LTLIBRARIES = libbrasero-woodim.la
libbrasero_woodim_la_SOURCES = burn-woodim.c burn-woodim.h
libbrasero_woodim_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_woodim_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)

#genisoimage
genisoimagedir = $(libdir)/brasero/plugins
genisoimage_LTLIBRARIES = libbrasero-genisoimage.la
libbrasero_genisoimage_la_SOURCES = burn-genisoimage.c burn-genisoimage.h 
libbrasero_genisoimage_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_genisoimage_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)

#readom
readomdir = $(libdir)/brasero/plugins
readom_LTLIBRARIES = libbrasero-readom.la
libbrasero_readom_la_SOURCES = burn-readom.c burn-readom.h
libbrasero_readom_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_readom_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)
