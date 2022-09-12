nautilus_dep = dependency('libnautilus-extension-4', required: get_option('nautilus'))

if nautilus_dep.found()
    nautilus_extensiondir = nautilus_dep.get_variable(
        pkgconfig: 'extensiondir',
        pkgconfig_define: ['libdir', libdir],
    )

    shared_module(
        'nautilus-brasero-extension',
        files('nautilus-burn-extension.c'),
        dependencies: [
            libbrasero_burn,
            libbrasero_media,
            nautilus_dep,
        ],
        include_directories: root_inc,
        install: true,
        install_dir: nautilus_extensiondir,
    )
endif