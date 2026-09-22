desktop_file = i18n.merge_file(
    input: 'brasero.desktop.in',
    output: 'brasero.desktop',
    type: 'desktop',
    po_dir: po_dir,
    install: true,
    install_dir: datadir / 'applications',
)

test(
    'valid-desktop-file',
    desktop_file_validate,
    args: desktop_file,
)

metainfo_file = i18n.merge_file(
    input: 'org.gnome.Brasero.metainfo.xml.in',
    output: 'org.gnome.Brasero.metainfo.xml',
    type: 'xml',
    po_dir: po_dir,
    install: true,
    install_dir: datadir / 'metainfo',
)

test(
    'valid-appstream-file',
    appstreamcli,
    args: [
        'validate',
        '--no-net',
        metainfo_file,
    ],
)

i18n.merge_file(
    input: 'mime/brasero.xml.in',
    output: 'brasero.xml',
    type: 'xml',
    po_dir: po_dir,
    install: true,
    install_dir: datadir / 'mime/packages',
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
