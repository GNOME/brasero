
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

#cdrecord
cdrecorddir = $(BRASERO_PLUGIN_DIRECTORY)
cdrecord_LTLIBRARIES = libbrasero-cdrecord.la
libbrasero_cdrecord_la_SOURCES = burn-cdrecord.c \
	burn-cdrtools.h 
libbrasero_cdrecord_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_cdrecord_la_LDFLAGS = -module -avoid-version

#mkisofs
mkisofsdir = $(BRASERO_PLUGIN_DIRECTORY)
mkisofs_LTLIBRARIES = libbrasero-mkisofs.la
libbrasero_mkisofs_la_SOURCES = burn-mkisofs.c \
	burn-cdrtools.h 
libbrasero_mkisofs_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_mkisofs_la_LDFLAGS = -module -avoid-version

#readcd
readcddir = $(BRASERO_PLUGIN_DIRECTORY)
readcd_LTLIBRARIES = libbrasero-readcd.la
libbrasero_readcd_la_SOURCES = burn-readcd.c \
	burn-cdrtools.h 
libbrasero_readcd_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_readcd_la_LDFLAGS = -module -avoid-version

#cdda2wav
cdda2wavdir = $(BRASERO_PLUGIN_DIRECTORY)
cdda2wav_LTLIBRARIES = libbrasero-cdda2wav.la
libbrasero_cdda2wav_la_SOURCES = burn-cdda2wav.c burn-cdrtools.h 
libbrasero_cdda2wav_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_cdda2wav_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
