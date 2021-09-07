sharedir = join_paths(get_option('prefix'), 'share')
iconsdir = join_paths(sharedir, 'icons', 'hicolor')

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file', appstream_util, args: [
    'validate-relax',
    join_paths(meson.current_source_dir(), 'com.github.rafostar.Clapper.metainfo.xml')
  ])
endif

install_data('com.github.rafostar.Clapper.svg',
  install_dir: join_paths(iconsdir, 'scalable', 'apps')
)
install_data('com.github.rafostar.Clapper-symbolic.svg',
  install_dir: join_paths(iconsdir, 'symbolic', 'apps')
)
install_data('com.github.rafostar.Clapper.gschema.xml',
  install_dir: join_paths(sharedir, 'glib-2.0', 'schemas')
)
install_data('com.github.rafostar.Clapper.xml',
  install_dir: join_paths(sharedir, 'mime', 'packages')
)
install_data('com.github.rafostar.Clapper.desktop',
  install_dir: join_paths(sharedir, 'applications')
)
install_data('com.github.rafostar.Clapper.metainfo.xml',
  install_dir: join_paths(sharedir, 'metainfo')
)
gnome.compile_resources('com.github.rafostar.Clapper.data',
  'com.github.rafostar.Clapper.data.gresource.xml',
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir,
)
