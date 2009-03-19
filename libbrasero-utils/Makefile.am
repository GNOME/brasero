
INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_SM_CFLAGS)

lib_LTLIBRARIES = \
	libbrasero-utils.la

libbrasero_utils_la_LIBADD =					\
	$(BRASERO_GLIB_LIBS)					\
	$(BRASERO_GTK_LIBS)					\
	$(BRASERO_SM_LIBS)

libbrasero_utils_la_LDFLAGS =					\
	-version-info $(LIBBRASERO_LT_VERSION)			\
	-no-undefined						\
	$(AM_LDFLAGS)
#	-export-symbols ./libbrasero-utils.symbols

libbrasero_utils_la_SOURCES =		\
	brasero-disc-message.c        \
	brasero-disc-message.h        \
	brasero-misc.c        \
	brasero-misc.h        \
	brasero-notify.c        \
	brasero-notify.h        \
	brasero-jacket-background.c        \
	brasero-jacket-background.h        \
	brasero-jacket-buffer.c        \
	brasero-jacket-buffer.h        \
	brasero-jacket-edit.c        \
	brasero-jacket-edit.h        \
	brasero-jacket-font.c        \
	brasero-jacket-font.h        \
	brasero-jacket-view.c        \
	brasero-jacket-view.h        \
	brasero-tool-color-picker.c        \
	brasero-tool-color-picker.h        \
	eggdesktopfile.c        \
	eggdesktopfile.h        \
	eggsmclient.c        \
	eggsmclient.h        \
	eggsmclient-private.h        \
	eggsmclient-xsmp.c

# EXTRA_DIST =			\
#	libbrasero-utils.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)
	
