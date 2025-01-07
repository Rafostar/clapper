appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file',
    appstream_util,
    args: [
      'validate-relax',
      '--nonet',
      join_paths(meson.current_source_dir(), 'metainfo', 'com.github.rafostar.Clapper.metainfo.xml'),
    ]
  )
endif

glib_compile_schemas = find_program('glib-compile-schemas', required: false)
if glib_compile_schemas.found()
  test('Validate gsettings schemas',
    glib_compile_schemas,
    args: [
      '--strict',
      '--dry-run',
      join_paths(meson.current_source_dir(), 'glib-2.0', 'schemas'),
    ]
  )
endif

install_subdir('icons',
  install_dir: join_paths(prefix, datadir)
)
install_subdir('metainfo',
  install_dir: join_paths(prefix, datadir)
)

is_linux = ['linux'].contains(host_machine.system())
is_windows = ['windows'].contains(host_machine.system())

if is_linux
  desktop_file = i18n.merge_file(
    input: 'applications/com.github.rafostar.Clapper.desktop.in',
    output: 'com.github.rafostar.Clapper.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(prefix, datadir, 'applications'),
  )
  desktop_utils = find_program('desktop-file-validate', required: false)
  if desktop_utils.found()
    test('Validate desktop file', desktop_utils, args: [desktop_file])
  endif
  subdir('dbus-1')
endif

if not is_windows
  install_subdir('mime',
    install_dir: join_paths(prefix, datadir)
  )
endif

subdir('glib-2.0/schemas')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: is_linux,
  update_mime_database: not is_windows,
)
