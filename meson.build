project(
    'brasero',
    'c',
    license: 'GPL-2.0-only',
    license_files: 'COPYING',
    meson_version: '>=1.4.0',
    version: '3.12.3',
)

library_version = '1.2.7'

fs = import('fs')
gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

opt_libburn = get_option('libburn')
opt_libisofs = get_option('libisofs')

error_message = 'The libisofs plugin requires libburn'
opt_libisofs = opt_libisofs.require(
    opt_libburn.allowed(),
    error_message: error_message,
)
opt_libburn = opt_libburn.enable_if(
    opt_libisofs.enabled(),
    error_message: error_message,
)

opt_playlist = get_option('playlist')
opt_search = get_option('search')

error_message = 'The search feature is only available with the playlist feature'
opt_search = opt_search.require(
    opt_playlist.allowed(),
    error_message: error_message,
)
opt_playlist = opt_playlist.enable_if(
    opt_search.enabled(),
    error_message: error_message,
)

cc = meson.get_compiler('c')

cairo = dependency('cairo')
gdk3 = dependency('gdk-3.0')
gdk_pixbuf2 = dependency('gdk-pixbuf-2.0')
gdk_x11_3 = dependency('gdk-x11-3.0')
gio2 = dependency('gio-2.0', version: '>=2.28.0')
glib2 = dependency('glib-2.0', version: '>=2.29.14')
gmodule2 = dependency('gmodule-2.0', version: '>=2.6.0')
gmodule_no_export2 = dependency('gmodule-no-export-2.0', version: '>=2.6.0')
gobject2 = dependency('gobject-2.0')
gstreamer1 = dependency('gstreamer-1.0', version: '>=0.11.92')
gstreamer_base1 = dependency('gstreamer-base-1.0', version: '>=0.11.92')
gstreamer_pbutils1 = dependency('gstreamer-pbutils-1.0', version: '>=0.11.92')
gstreamer_tag1 = dependency('gstreamer-tag-1.0', version: '>=0.11.92')
gstreamer_video1 = dependency(
    'gstreamer-video-1.0',
    required: get_option('preview'),
    version: '>=0.11.92',
)
gthread2 = dependency('gthread-2.0', version: '>=2.6.0')
gtk3 = dependency('gtk+-3.0')
libburn1 = dependency('libburn-1', required: opt_libburn, version: '>=0.4.0')
libcanberra_gtk3 = dependency('libcanberra-gtk3', version: '>=0.1')
libisofs1 = dependency('libisofs-1', required: opt_libisofs, version: '>=0.6.4')
libm = cc.find_library('m')
libnotify = dependency('libnotify', version: '>=0.6.1')
libxml2 = dependency('libxml-2.0', version: '>=2.6.0')
totem_plparser = dependency(
    'totem-plparser',
    required: opt_playlist,
    version: '>=2.29.1',
)
tracker_sparql3 = dependency(
    'tracker-sparql-3.0',
    required: opt_search,
    version: '>=0.10.0',
)

appstreamcli = find_program('appstreamcli', disabler: true, required: false)
desktop_file_validate = find_program(
    'desktop-file-validate',
    disabler: true,
    required: false,
)
glib_compile_schemas = find_program(
    gio2.get_variable('glib_compile_schemas'),
    disabler: true,
    required: false,
)
intltool_merge = find_program('intltool-merge')

prefix = get_option('prefix')
datadir = get_option('datadir')
libdir = get_option('libdir')
localedir = get_option('localedir')
plugindir = libdir / 'brasero3/plugins'

version = meson.project_version()
version_split = version.split('.')

conf = configuration_data(
    {
        'BRASERO_MAJOR_VERSION': version_split[0],
        'BRASERO_MINOR_VERSION': version_split[1],
        'BRASERO_SUB': version_split[2],
    },
)

library_version_split = library_version.split('.')

conf_lib = configuration_data(
    {
        # does this logic work for platforms other than Linux?
        'LT_CURRENT': library_version_split[0].to_int() + library_version_split[1].to_int(),
        'LT_REVISION': library_version_split[2],
        'LT_AGE': library_version_split[1],
        'prefix': prefix,
    },
)
conf_lib.merge_from(conf)

conf.set_quoted('BRASERO_DATADIR', prefix / datadir / 'brasero')
conf.set_quoted('BRASERO_PLUGIN_DIRECTORY', prefix / plugindir)
conf.set('BUILD_INOTIFY', cc.has_header('sys/inotify.h'))
if totem_plparser.found()
    conf.set10('BUILD_PLAYLIST', true)
endif
conf.set('BUILD_PREVIEW', gstreamer_video1.found())
conf.set('BUILD_SEARCH', tracker_sparql3.found())
conf.set('BUILD_TRACKER', tracker_sparql3.found())
# is it ever desirable not to have this?
conf.set('ENABLE_NLS', true)
conf.set_quoted('GETTEXT_PACKAGE', 'brasero')
# these two are not required by brasero, but by libisofs
conf.set('HAVE_INTTYPES_H', cc.has_header('inttypes.h'))
conf.set(
    'HAVE_STDINT_H',
    cc.has_header('stdint.h', required: opt_libisofs.enabled()),
)
conf.set('HAVE_TRACKER3', true)
conf.set_quoted('PACKAGE_LOCALE_DIR', prefix / localedir)
conf.set_quoted('VERSION', version)
configure_file(configuration: conf, output: 'config.h')

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
root_inc = include_directories('.')

subdir('po')
subdir('data')
if get_option('yelp')
    subdir('help')
endif
subdir('libbrasero-media')
subdir('libbrasero-utils')
subdir('libbrasero-burn')
subdir('src')
subdir('plugins')
subdir('docs')
# TODO: port to newer libnautilus-extension
# subdir('nautilus')

gnome.post_install(
    glib_compile_schemas: true,
    gtk_update_icon_cache: true,
    update_desktop_database: true,
    update_mime_database: true,
)
