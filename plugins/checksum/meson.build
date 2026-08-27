common_deps = [
    glib2,
    gmodule_no_export2,
    gobject2,
    libbrasero_burn,
    libbrasero_media,
]

shared_module(
    'brasero-checksum-file',
    files('burn-checksum-files.c', 'burn-volume-read.c'),
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

shared_module(
    'brasero-checksum',
    'burn-checksum-image.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

unset_variable('common_deps')
