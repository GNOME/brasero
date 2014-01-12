SUBDIRS = icons mime

convertdir=$(datadir)/GConf/gsettings
convert_DATA=brasero.convert

gsettings_SCHEMAS = org.gnome.brasero.gschema.xml
@GSETTINGS_RULES@

desktopdir=$(datadir)/applications
desktop_in_files =	brasero.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

desktopdir=$(datadir)/Thunar
desktop_in_files =	thunar-sendto-brasero.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

appdatadir = $(datadir)/appdata
appdata_in_files = brasero.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

CLEANFILES =		$(appdata_DATA)	\
			$(desktop_DATA)



EXTRA_DIST = 		$(appdata_in_files)	\
			$(desktop_in_files)	\
			$(gsettings_SCHEMAS)	\
			$(convert_DATA)

UPDATE_DESKTOP = \
	if [ -f $(DESTDIR)$(datadir)/applications/defaults.list ] ; then \
		if which update-desktop-database>/dev/null 2>&1 ; then \
			update-desktop-database; \
		fi \
	fi

install-data-hook: 
if UPDATE_CACHES
	$(UPDATE_DESKTOP)
else
	@true
endif

uninstall-hook: 
if UPDATE_CACHES
	$(UPDATE_DESKTOP)
else
	@true
endif

-include $(top_srcdir)/git.mk
