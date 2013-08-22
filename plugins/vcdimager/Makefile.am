
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
	$(DISABLE_DEPRECATED)					\
	$(BRASERO_GLIB_CFLAGS)					\
	$(BRASERO_LIBXML_CFLAGS)

vcdimagerdir = $(BRASERO_PLUGIN_DIRECTORY)
vcdimager_LTLIBRARIES = libbrasero-vcdimager.la

libbrasero_vcdimager_la_SOURCES = burn-vcdimager.c 
libbrasero_vcdimager_la_LIBADD = ../../libbrasero-burn/libbrasero-burn3.la $(BRASERO_GLIB_LIBS) $(BRASERO_LIBXML_LIBS)
libbrasero_vcdimager_la_LDFLAGS = -module -avoid-version


-include $(top_srcdir)/git.mk
