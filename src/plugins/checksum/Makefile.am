
DISABLE_DEPRECATED = -DG_DISABLE_DEPRECATED

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/md5						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           		\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   		\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    	\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         	\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_BASE_CFLAGS)

AM_CFLAGS = -g

checksumdir = $(libdir)/brasero/plugins
checksum_LTLIBRARIES = libbrasero-checksum.la
libbrasero_checksum_la_SOURCES = burn-checksum-image.c	\
				 burn-checksum-image.h

libbrasero_checksum_la_LDFLAGS = -module -avoid-version
libbrasero_checksum_la_LIBADD = $(BRASERO_BASE_LIBS)

checksumfiledir = $(libdir)/brasero/plugins
checksumfile_LTLIBRARIES = libbrasero-checksum-file.la
libbrasero_checksum_file_la_SOURCES = burn-checksum-files.c	\
				      burn-checksum-files.h

libbrasero_checksum_file_la_LDFLAGS = -module -avoid-version
libbrasero_checksum_file_la_LIBADD = $(BRASERO_BASE_LIBS)
