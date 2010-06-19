
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
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)				\
	$(BRASERO_GLIB_CFLAGS)

#growisofs
growisofsdir = $(libdir)/brasero/plugins
growisofs_LTLIBRARIES = libbrasero-growisofs.la
libbrasero_growisofs_la_SOURCES = burn-growisofs.c \
	burn-growisofs-common.h 
libbrasero_growisofs_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_growisofs_la_LDFLAGS = -module -avoid-version

#dvdrwformat
dvdrwformatdir = $(libdir)/brasero/plugins
dvdrwformat_LTLIBRARIES = libbrasero-dvdrwformat.la
libbrasero_dvdrwformat_la_SOURCES = burn-dvd-rw-format.c 
libbrasero_dvdrwformat_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_dvdrwformat_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
