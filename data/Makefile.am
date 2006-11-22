SUBDIRS = icons

@INTLTOOL_SCHEMAS_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
schemas_in_files = brasero.schemas.in
schemas_DATA = $(schemas_in_files:.schemas.in=.schemas)


@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/applications
desktop_in_files = brasero.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_RULE@

MIMEFILES = brasero.xml.in
mimedir = $(datadir)/mime/packages
mime_DATA = brasero.xml


IMAGES=	logo.png

CLEANFILES =	$(desktop_DATA)\
			$(mime_DATA)

EXTRA_DIST = $(desktop_in_files)\
			$(MIMEFILES)\
			$(IMAGES)

braserodistpixdir = $(datadir)/brasero/
braserodistpix_DATA = $(IMAGES)

UPDATE_MIME = \
	if [ -f $(DESTDIR)$(datadir)/mime/packages/freedesktop.org.xml ] ; then \
		if which update-mime-database>/dev/null 2>&1; then \
			update-mime-database $(DESTDIR)$(datadir)/mime; \
		fi \
	fi

UPDATE_DESKTOP = \
	if which update-desktop-database>/dev/null 2>&1 ; then \
		update-desktop-database; \
	fi

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local: $(schemas_DATA)
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/data/$$p ; \
		done \
	fi
else
install-data-local:
endif

install-data-hook: 
if UPDATE_CACHES
	$(UPDATE_MIME)
	$(UPDATE_DESKTOP)
else
	@true
endif

uninstall-hook: 
if UPDATE_CACHES
	$(UPDATE_MIME)
	$(UPDATE_DESKTOP)
else
	@true
endif
