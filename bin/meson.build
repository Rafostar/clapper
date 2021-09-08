clapper_apps = ['', 'Remote', 'Daemon']

foreach id_postfix : clapper_apps
  app_postfix = (id_postfix != '') ? '.' + id_postfix : ''

  bin_conf = configuration_data()
  bin_conf.set('GJS', find_program('gjs').path())
  bin_conf.set('PACKAGE_NAME', meson.project_name())
  bin_conf.set('PACKAGE_VERSION', meson.project_version())
  bin_conf.set('ID_POSTFIX', id_postfix)
  bin_conf.set('prefix', get_option('prefix'))
  bin_conf.set('libdir', libdir)
  bin_conf.set('datadir', datadir)

  configure_file(
    input: 'com.github.rafostar.Clapper.in',
    output: 'com.github.rafostar.Clapper' + app_postfix,
    configuration: bin_conf,
    install: true,
    install_dir: bindir,
    install_mode: 'rwxr-xr-x'
  )
endforeach

clapper_symlink_cmd = 'ln -fs @0@ $DESTDIR@1@'.format(
  'com.github.rafostar.Clapper',
  join_paths(bindir, 'clapper')
)
meson.add_install_script('sh', '-c', clapper_symlink_cmd)
