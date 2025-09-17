test(
    'valid-desktop-file',
    desktop_file_validate,
    args: custom_target(
        command: [
            intltool_merge,
            '--desktop-style',
            po_dir,
            '@INPUT@',
            '@OUTPUT@',
        ],
        input: 'brasero.desktop.in',
        install: true,
        install_dir: datadir / 'applications',
        output: '@BASENAME@',
    ),
)

test(
    'valid-appstream-file',
    appstreamcli,
    args: [
        'validate',
        '--strict',
        custom_target(
            command: [
                intltool_merge,
                '--xml-style',
                po_dir,
                '@INPUT@',
                '@OUTPUT@',
            ],
            input: 'brasero.appdata.xml.in',
            install: true,
            install_dir: datadir / 'metainfo',
            output: '@BASENAME@',
        ),
    ],
)

custom_target(
    command: [intltool_merge, '--xml-style', po_dir, '@INPUT@', '@OUTPUT@'],
    input: 'mime/brasero.xml.in',
    install: true,
    install_dir: datadir / 'mime/packages',
    output: '@BASENAME@',
)

schema_file = files('org.gnome.brasero.gschema.xml')[0]
gnome.compile_schemas(depend_files: schema_file)
install_data(schema_file, install_dir: datadir / 'glib-2.0/schemas')
test(
    'valid-gschema',
    glib_compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()],
)

subdir('icons')
