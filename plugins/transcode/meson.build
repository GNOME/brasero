common_deps = [glib2, gmodule_no_export2, gstreamer1, libbrasero_burn]

shared_module(
    'brasero-normalize',
    'burn-normalize.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

shared_module(
    'brasero-transcode',
    'burn-transcode.c',
    dependencies: common_deps + libbrasero_media,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

shared_module(
    'brasero-vob',
    'burn-vob.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

unset_variable('common_deps')
