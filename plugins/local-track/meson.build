common_deps = [gio2, glib2, gmodule_no_export2, gobject2, libbrasero_burn]

shared_module(
    'brasero-burn-uri',
    'burn-uri.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

shared_module(
    'brasero-local-track',
    'burn-local-image.c',
    dependencies: common_deps + libbrasero_media,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

unset_variable('common_deps')
