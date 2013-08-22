
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
	$(BRASERO_LIBISOFS_CFLAGS)			\
	$(BRASERO_LIBBURN_CFLAGS)			\
	$(BRASERO_GLIB_CFLAGS)

#libburn
libburndir = $(BRASERO_PLUGIN_DIRECTORY)
libburn_LTLIBRARIES = libbrasero-libburn.la
libbrasero_libburn_la_SOURCES = burn-libburn.c 		\
				burn-libburn-common.c	\
				burn-libburn-common.h  \
				burn-libburnia.h 
libbrasero_libburn_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_LIBBURNIA_LIBS)
libbrasero_libburn_la_LDFLAGS = -module -avoid-version

#libisofs (apparently libisofs needs one libburn function)
libisofsdir = $(BRASERO_PLUGIN_DIRECTORY)
libisofs_LTLIBRARIES = libbrasero-libisofs.la
libbrasero_libisofs_la_SOURCES = burn-libisofs.c                       \
	burn-libburn-common.c burn-libburn-common.h			\
	burn-libburnia.h 
libbrasero_libisofs_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_LIBBURNIA_LIBS)
libbrasero_libisofs_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
