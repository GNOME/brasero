common_deps = [glib2, gobject2, libbrasero_burn, libburn1]

common_deps += [
    declare_dependency(
        link_with: static_library(
            'common',
            'burn-libburn-common.c',
            dependencies: common_deps,
            include_directories: root_inc,
        ),
    ),
    gmodule_no_export2,
]

shared_module(
    'brasero-libburn',
    'burn-libburn.c',
    dependencies: common_deps,
    include_directories: root_inc,
    install: true,
    install_dir: plugindir,
    name_prefix: 'lib',
)

if libisofs1.found() and conf.get('HAVE_STDINT_H')
    shared_module(
        'brasero-libisofs',
        'burn-libisofs.c',
        dependencies: common_deps + libisofs1,
        include_directories: root_inc,
        install: true,
        install_dir: plugindir,
        name_prefix: 'lib',
    )
endif

unset_variable('common_deps')
