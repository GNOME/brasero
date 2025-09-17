# Linux interface
sg_sources = files('scsi-sg.c')

# FreeBSD interface
camlib_sources = files('scsi-cam.c')

# NetBSD interface
netbsd_sources = files('scsi-netbsd.c')

# SunOS interface
uscsi_sources = files('scsi-uscsi.c')

scsi_dep = []

if cc.has_header('scsi/sg.h')
    scsi_sources = sg_sources
elif cc.has_header('camlib.h')
    scsi_sources = camlib_sources
    scsi_dep = cc.find_library('cam')
elif cc.has_header('sys/scsiio.h')
    scsi_sources = netbsd_sources
elif cc.has_header('sys/scsi/impl/uscsi.h')
    scsi_sources = uscsi_sources
else
    error(
        'This system is unsupported: Brasero does not have a SCSI implementation for it',
    )
endif

libbrasero_media = library(
    'brasero-media3',
    files(
        'brasero-drive-selection.c',
        'brasero-drive.c',
        'brasero-gio-operation.c',
        'brasero-media.c',
        'brasero-medium-monitor.c',
        'brasero-medium-selection.c',
        'brasero-medium.c',
        'brasero-units.c',
        'brasero-volume.c',
        'burn-iso-field.c',
        'burn-iso9660.c',
        'burn-susp.c',
        'burn-volume-source.c',
        'burn-volume.c',
        'scsi-error.c',
        'scsi-get-configuration.c',
        'scsi-get-performance.c',
        'scsi-inquiry.c',
        'scsi-mech-status.c',
        'scsi-mode-select.c',
        'scsi-mode-sense.c',
        'scsi-prevent-allow-medium-removal.c',
        'scsi-read-cd.c',
        'scsi-read-disc-info.c',
        'scsi-read-disc-structure.c',
        'scsi-read-format-capacities.c',
        'scsi-read-toc-pma-atip.c',
        'scsi-read-track-information.c',
        'scsi-read10.c',
        'scsi-sense-data.c',
        'scsi-test-unit-ready.c',
    ),
    scsi_sources,
    dependencies: [gdk3, gio2, glib2, gobject2, gtk3, scsi_dep],
    include_directories: root_inc,
    install: true,
    version: library_version,
)

install_headers(
    files(
        'brasero-drive-selection.h',
        'brasero-drive.h',
        'brasero-medium-monitor.h',
        'brasero-medium-selection.h',
        'brasero-medium.h',
        'brasero-units.h',
        'brasero-volume.h',
    ),
    configure_file(
        configuration: conf_lib,
        input: 'brasero-media.h.in',
        output: '@BASENAME@',
    ),
    subdir: 'brasero3',
)

pkgconfig.generate(
    libbrasero_media,
    name: 'libbrasero-media3',
    subdirs: 'brasero3',
)

libbrasero_media = declare_dependency(
    dependencies: gtk3,
    include_directories: '.',
    link_with: libbrasero_media,
)
meson.override_dependency('libbrasero-media3', libbrasero_media)
