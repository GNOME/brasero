test_c_args = [
    '-DTEST_PLUGIN_DIR="@0@"'.format(meson.project_build_root() / 'plugins' / 'libburnia'),
]

if libburn1.found()
    test_c_args += ['-DHAVE_LIBBURN=1']
endif
if libisofs1.found() and conf.get('HAVE_STDINT_H')
    test_c_args += ['-DHAVE_LIBISOFS=1']
endif

test_libburnia = executable(
    'test-libburnia',
    'test-libburnia.c',
    c_args: test_c_args,
    dependencies: [glib2, gobject2, gio2, libbrasero_burn],
    include_directories: root_inc,
)

xvfb_run = find_program('xvfb-run', required: false)

test_env = [
    'GSETTINGS_BACKEND=memory',
    'GSETTINGS_SCHEMA_DIR=' + meson.project_build_root() / 'data',
    'LD_LIBRARY_PATH=' + meson.project_build_root() / 'libbrasero-burn:' + meson.project_build_root() / 'libbrasero-media:' + meson.project_build_root() / 'libbrasero-utils',
]

if xvfb_run.found()
    test(
        'libburnia',
        xvfb_run,
        args: ['-a', test_libburnia],
        env: test_env,
    )
else
    test(
        'libburnia',
        test_libburnia,
        env: test_env,
    )
endif
