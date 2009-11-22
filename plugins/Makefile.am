SUBDIRS = transcode dvdcss checksum local-track dvdauthor vcdimager audio2cue

if BUILD_LIBBURNIA
SUBDIRS += libburnia
endif

if BUILD_CDRKIT
SUBDIRS += cdrkit
endif

if BUILD_CDRTOOLS
SUBDIRS += cdrtools
endif

if BUILD_CDRDAO
SUBDIRS += cdrdao
endif

if BUILD_GROWISOFS
SUBDIRS += growisofs
endif

-include $(top_srcdir)/git.mk
