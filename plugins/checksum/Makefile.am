
AM_CPPFLAGS = \
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libbrasero-media/					\
	-I$(top_builddir)/libbrasero-media/				\
	-I$(top_srcdir)/libbrasero-burn				\
	-I$(top_builddir)/libbrasero-burn/				\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_GLIB_CFLAGS)				\
	$(BRASERO_GTK_CFLAGS)

checksumdir = $(BRASERO_PLUGIN_DIRECTORY)
checksum_LTLIBRARIES = libbrasero-checksum.la
libbrasero_checksum_la_SOURCES = burn-checksum-image.c	

libbrasero_checksum_la_LDFLAGS = -module -avoid-version
libbrasero_checksum_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)

checksumfiledir = $(BRASERO_PLUGIN_DIRECTORY)
checksumfile_LTLIBRARIES = libbrasero-checksum-file.la
libbrasero_checksum_file_la_SOURCES = burn-checksum-files.c	\
				      burn-volume-read.c  \
				      burn-volume-read.h

libbrasero_checksum_file_la_LDFLAGS = -module -avoid-version
libbrasero_checksum_file_la_LIBADD = ../../libbrasero-media/libbrasero-media3.la ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_GTK_LIBS)

-include $(top_srcdir)/git.mk
