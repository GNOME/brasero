common_deps = [
    glib2,
    gmodule_no_export2,
    gobject2,
    libbrasero_burn,
    libbrasero_media,
]

shared_module(
    'brasero-dvdrwformat',
    'burn-dvd-rw-format.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

shared_module(
    'brasero-growisofs',
    'burn-growisofs.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

unset_variable('common_deps')
