
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
	$(BRASERO_GLIB_CFLAGS)				\
	$(BRASERO_GCONF_CFLAGS)

#cdrecord
cdrecorddir = $(libdir)/brasero/plugins
cdrecord_LTLIBRARIES = libbrasero-cdrecord.la
libbrasero_cdrecord_la_SOURCES = burn-cdrecord.c burn-cdrecord.h \
	burn-cdrtools.h 
libbrasero_cdrecord_la_LIBADD = $(BRASERO_GLIB_LIBS) $(BRASERO_GCONF_LIBS)
libbrasero_cdrecord_la_LDFLAGS = -module -avoid-version

#mkisofs
mkisofsdir = $(libdir)/brasero/plugins
mkisofs_LTLIBRARIES = libbrasero-mkisofs.la
libbrasero_mkisofs_la_SOURCES = burn-mkisofs.c burn-mkisofs.h \
	burn-cdrtools.h 
libbrasero_mkisofs_la_LIBADD = $(BRASERO_GLIB_LIBS)
libbrasero_mkisofs_la_LDFLAGS = -module -avoid-version

#readcd
readcddir = $(libdir)/brasero/plugins
readcd_LTLIBRARIES = libbrasero-readcd.la
libbrasero_readcd_la_SOURCES = burn-readcd.c burn-readcd.h \
	burn-cdrtools.h 
libbrasero_readcd_la_LIBADD = $(BRASERO_GLIB_LIBS)
libbrasero_readcd_la_LDFLAGS = -module -avoid-version
