
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
	$(BRASERO_LIBISOFS_CFLAGS)			\
	$(BRASERO_LIBBURN_CFLAGS)			\
	$(BRASERO_BASE_CFLAGS)

#libburn
libburndir = $(libdir)/brasero/plugins
libburn_LTLIBRARIES = libbrasero-libburn.la
libbrasero_libburn_la_SOURCES = burn-libburn.c 		\
				burn-libburn.h		\
				burn-libburn-common.c	\
				burn-libburn-common.h  \
				burn-libburnia.h 
libbrasero_libburn_la_LIBADD = $(BRASERO_BASE_LIBS) $(BRASERO_LIBBURNIA_LIBS)
libbrasero_libburn_la_LDFLAGS = -module -avoid-version

#libisofs (apparently libisofs needs one libburn function)
libisofsdir = $(libdir)/brasero/plugins
libisofs_LTLIBRARIES = libbrasero-libisofs.la
libbrasero_libisofs_la_SOURCES = burn-libisofs.c burn-libisofs.h	\
	burn-libburn-common.c burn-libburn-common.h			\
	burn-libburnia.h 
libbrasero_libisofs_la_LIBADD = $(BRASERO_BASE_LIBS) $(BRASERO_LIBBURNIA_LIBS)
libbrasero_libisofs_la_LDFLAGS = -module -avoid-version
