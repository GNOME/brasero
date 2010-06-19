MIMEFILES = brasero.xml.in
mimedir = $(datadir)/mime/packages
mime_DATA = brasero.xml

@INTLTOOL_XML_RULE@

CLEANFILES =	$(mime_DATA)

EXTRA_DIST = 	$(MIMEFILES)

UPDATE_MIME = \
	if [ -f $(DESTDIR)$(datadir)/mime/packages/freedesktop.org.xml ] ; then \
		if which update-mime-database>/dev/null 2>&1; then \
			update-mime-database $(DESTDIR)$(datadir)/mime; \
		fi \
	fi

install-data-hook: 
if UPDATE_CACHES
	$(UPDATE_MIME)
else
	@true
endif

uninstall-hook: 
if UPDATE_CACHES
	$(UPDATE_MIME)
else
	@true
endif

-include $(top_srcdir)/git.mk
