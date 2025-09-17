shared_library(
    'nautilus-burn',
    files('nautilus-burn-extension.c'),
    dependencies: [
        dependency('atk', disabler: true, required: false),
        gdk_x11_3,
        libbrasero_burn,
        libbrasero_media,
        libbrasero_utils,
        dependency('libnautilus-extension', disabler: true, required: false),
    ],
    include_directories: root_inc,
)
