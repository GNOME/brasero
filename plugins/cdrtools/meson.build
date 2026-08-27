common_deps = [glib2, gmodule_no_export2, libbrasero_burn]

shared_module(
    'brasero-cdda2wav',
    'burn-cdda2wav.c',
    dependencies: common_deps + [gobject2, libbrasero_media],
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

shared_module(
    'brasero-cdrecord',
    'burn-cdrecord.c',
    dependencies: common_deps + [gobject2, libbrasero_media],
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

shared_module(
    'brasero-mkisofs',
    'burn-mkisofs.c',
    dependencies: common_deps + gobject2,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

shared_module(
    'brasero-readcd',
    'burn-readcd.c',
    dependencies: common_deps + libbrasero_media,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

unset_variable('common_deps')
