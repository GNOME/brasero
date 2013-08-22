
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

#growisofs
growisofsdir = $(BRASERO_PLUGIN_DIRECTORY)
growisofs_LTLIBRARIES = libbrasero-growisofs.la
libbrasero_growisofs_la_SOURCES = burn-growisofs.c \
	burn-growisofs-common.h 
libbrasero_growisofs_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_growisofs_la_LDFLAGS = -module -avoid-version

#dvdrwformat
dvdrwformatdir = $(BRASERO_PLUGIN_DIRECTORY)
dvdrwformat_LTLIBRARIES = libbrasero-dvdrwformat.la
libbrasero_dvdrwformat_la_SOURCES = burn-dvd-rw-format.c 
libbrasero_dvdrwformat_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS)
libbrasero_dvdrwformat_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
