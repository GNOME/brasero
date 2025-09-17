common_deps = [glib2, gmodule_no_export2, libbrasero_burn]

shared_module(
    'brasero-genisoimage',
    'burn-genisoimage.c',
    dependencies: common_deps + gobject2,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

shared_module(
    'brasero-readom',
    'burn-readom.c',
    dependencies: common_deps + libbrasero_media,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

shared_module(
    'brasero-wodim',
    'burn-wodim.c',
    dependencies: common_deps + [gobject2, libbrasero_media],
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
)

unset_variable('common_deps')
