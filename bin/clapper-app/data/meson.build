if not ['linux'].contains(host_machine.system())
  subdir_done()
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file',
    appstream_util,
    args: [
      'validate-relax',
      '--nonet',
      join_paths(meson.current_source_dir(), 'com.github.rafostar.Clapper.metainfo.xml'),
    ]
  )
endif

install_subdir('icons',
  install_dir: join_paths(prefix, datadir)
)
install_subdir('mime',
  install_dir: join_paths(prefix, datadir)
)
install_subdir('applications',
  install_dir: join_paths(prefix, datadir)
)
install_subdir('metainfo',
  install_dir: join_paths(prefix, datadir)
)

subdir('dbus-1')
