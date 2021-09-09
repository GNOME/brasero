## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=libbrasero-burn

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=$(top_srcdir)/libbrasero-burn $(top_builddir)/libbrasero-burn

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS=--rebuild-types

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=--extra-dir=../libbrasero-media/html

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/libbrasero-burn/*.h
CFILE_GLOB=$(top_srcdir)/libbrasero-burn/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=	libbrasero-marshal.h	\
		brasero-caps-burn.h		\
		brasero-cover.h		\
		brasero-data-project.h		\
		brasero-data-session.h		\
		brasero-data-tree-model.h		\
		brasero-data-vfs.h		\
		brasero-dest-selection.h		\
		brasero-drive-properties.h		\
		brasero-file-monitor.h		\
		brasero-file-node.h		\
		brasero-filtered-uri.h		\
		brasero-image-properties.h		\
		brasero-image-type-chooser.h		\
		brasero-medium-properties.h		\
		brasero-plugin.h		\
		brasero-plugin-information.h		\
		brasero-plugin-registration.h		\
		brasero-progress.h		\
		brasero-session-helper.h		\
		brasero-src-image.h		\
		brasero-src-selection.h		\
		brasero-status-dialog.h		\
		brasero-tool-dialog-private.h		\
		brasero-track-type-private.h		\
		brasero-tray.h		\
		brasero-video-options.h		\
		brasero-xfer.h		\
		burn-basics.h		\
		burn-caps.h		\
		burn-dbus.h		\
		burn-debug.h		\
		burn-image-format.h		\
		burn-job.h		\
		burn-mkisofs-base.h		\
		burn-plugin-manager.h		\
		burn-process.h		\
		burn-task-ctx.h		\
		burn-task.h		\
		burn-task-item.h


# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=version.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. AM_CPPFLAGS=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
AM_CPPFLAGS=								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	$(BRASERO_GLIB_CFLAGS)						\
	$(BRASERO_IO_CFLAGS)						\
	$(BRASERO_GTK_CFLAGS)

GTKDOC_LIBS=$(top_builddir)/libbrasero-media/libbrasero-media3.la $(top_builddir)/libbrasero-burn/libbrasero-burn3.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Comment this out if you want your docs-status tested during 'make check'
#TESTS = $(GTKDOC_CHECK)



-include $(top_srcdir)/git.mk
