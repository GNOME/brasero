
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

#wodim
wodimdir = $(libdir)/brasero/plugins
wodim_LTLIBRARIES = libbrasero-wodim.la
libbrasero_wodim_la_SOURCES = burn-wodim.c \
	burn-cdrkit.h 
libbrasero_wodim_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-media/libbrasero-media.la ../../libbrasero-burn/libbrasero-burn.la
libbrasero_wodim_la_LDFLAGS = -module -avoid-version

#genisoimage
genisoimagedir = $(libdir)/brasero/plugins
genisoimage_LTLIBRARIES = libbrasero-genisoimage.la
libbrasero_genisoimage_la_SOURCES = burn-genisoimage.c \
	burn-cdrkit.h 
libbrasero_genisoimage_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-burn/libbrasero-burn.la
libbrasero_genisoimage_la_LDFLAGS = -module -avoid-version

#readom
readomdir = $(libdir)/brasero/plugins
readom_LTLIBRARIES = libbrasero-readom.la
libbrasero_readom_la_SOURCES = burn-readom.c \
	burn-cdrkit.h 
libbrasero_readom_la_LIBADD = $(BRASERO_GLIB_LIBS) ../../libbrasero-media/libbrasero-media.la ../../libbrasero-burn/libbrasero-burn.la
libbrasero_readom_la_LDFLAGS = -module -avoid-version

-include $(top_srcdir)/git.mk
