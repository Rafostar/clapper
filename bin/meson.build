clapper_apps = ['', 'Remote', 'Daemon']

foreach id_postfix : clapper_apps
  app_postfix = (id_postfix != '') ? '.' + id_postfix : ''
  template_type = (id_postfix != '') ? '.Subprocess' : ''

  bin_conf = configuration_data()
  bin_conf.set('GJS', find_program('gjs').path())
  bin_conf.set('PACKAGE_NAME', meson.project_name())
  bin_conf.set('PACKAGE_VERSION', meson.project_version())
  bin_conf.set('ID_POSTFIX', id_postfix)
  bin_conf.set('prefix', get_option('prefix'))
  bin_conf.set('libdir', join_paths(get_option('prefix'), get_option('libdir')))
  bin_conf.set('datadir', join_paths(get_option('prefix'), get_option('datadir')))

  configure_file(
    input: 'com.github.rafostar.Clapper' + template_type + '.in',
    output: 'com.github.rafostar.Clapper' + app_postfix,
    configuration: bin_conf,
    install: true,
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x'
  )
endforeach

clapper_symlink_cmd = 'ln -fs @0@ @1@'.format(
  'com.github.rafostar.Clapper',
  join_paths(get_option('prefix'), get_option('bindir'), 'clapper')
)
meson.add_install_script('sh', '-c', clapper_symlink_cmd)
