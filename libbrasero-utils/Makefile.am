
INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-DBRASERO_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DBRASERO_PREFIX=\"$(prefix)\"           			\
	-DBRASERO_SYSCONFDIR=\"$(sysconfdir)\"   			\
	-DBRASERO_DATADIR=\"$(datadir)/brasero\"     	    		\
	-DBRASERO_LIBDIR=\"$(libdir)\"  	         		\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_GIO_CFLAGS)						\
	$(BRASERO_PL_PARSER_CFLAGS)					\
	$(BRASERO_GSTREAMER_CFLAGS)					\
	$(BRASERO_GSTREAMER_BASE_CFLAGS)				\
	$(BRASERO_GTK_CFLAGS)						\
	$(BRASERO_DBUS_CFLAGS)


lib_LTLIBRARIES = \
	libbrasero-utils@BRASERO_LIBRARY_SUFFIX@.la

libbrasero_utils@BRASERO_LIBRARY_SUFFIX@_la_LIBADD =					\
	$(BRASERO_GLIB_LIBS)					\
	$(BRASERO_GIO_LIBS)		\
	$(BRASERO_DBUS_LIBS)		\
	$(BRASERO_GSTREAMER_LIBS)	\
	$(BRASERO_GSTREAMER_BASE_LIBS)	\
	$(BRASERO_PL_PARSER_LIBS)	\
	$(BRASERO_GTK_LIBS)

libbrasero_utils@BRASERO_LIBRARY_SUFFIX@_la_LDFLAGS =					\
	-version-info $(LIBBRASERO_LT_VERSION)			\
	-no-undefined
#	-export-symbols ./libbrasero-utils.symbols

libbrasero_utils@BRASERO_LIBRARY_SUFFIX@_la_SOURCES =		\
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
	brasero-async-task-manager.c        \
	brasero-async-task-manager.h        \
	brasero-io.c        \
	brasero-io.h        \
	brasero-metadata.c        \
	brasero-metadata.h        \
	brasero-pk.c        \
	brasero-pk.h

# EXTRA_DIST =			\
#	libbrasero-utils.symbols

CLEANFILES =			\
	$(RECMARSHALFILES)

-include $(top_srcdir)/git.mk
