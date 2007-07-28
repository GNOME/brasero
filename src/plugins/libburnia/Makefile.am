
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

#libburn
libburndir = $(libdir)/brasero/plugins
libburn_LTLIBRARIES = libbrasero-libburn.la
libbrasero_libburn_la_SOURCES = burn-libburn.c 		\
				burn-libburn.h		\
				burn-libburn-common.c	\
				burn-libburn-common.h 
libbrasero_libburn_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_libburn_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)

#libisofs
libisofsdir = $(libdir)/brasero/plugins
libisofs_LTLIBRARIES = libbrasero-libisofs.la
libbrasero_libisofs_la_SOURCES = burn-libisofs.c burn-libisofs.h
libbrasero_libisofs_la_LIBADD = $(BRASERO_BASE_LIBS)
libbrasero_libisofs_la_LDFLAGS = -module -avoid-version $(AM_LDFLAGS)

#libcdread
libcdreaddir = $(libdir)/brasero/plugins
