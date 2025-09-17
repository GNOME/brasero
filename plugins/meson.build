subdir('audio2cue')
if get_option('cdrdao')
    subdir('cdrdao')
endif
if get_option('cdrkit')
    subdir('cdrkit')
endif
if get_option('cdrtools')
    subdir('cdrtools')
endif
subdir('checksum')
if get_option('dvdauthor')
    subdir('dvdauthor')
endif
if get_option('dvdcss')
    subdir('dvdcss')
endif
if get_option('growisofs')
    subdir('growisofs')
endif
subdir('libburnia', if_found: libburn1)
subdir('local-track')
subdir('transcode')
if get_option('vcdimager')
    subdir('vcdimager')
endif
