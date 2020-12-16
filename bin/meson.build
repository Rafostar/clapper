clapper_apps = [ '', 'Remote' ]

foreach id_postfix : clapper_apps
  bin_conf = configuration_data()
  bin_conf.set('GJS', find_program('gjs').path())
  bin_conf.set('PROJECT_NAME', meson.project_name())
  bin_conf.set('PACKAGE_NAME', meson.project_name() + id_postfix)
  bin_conf.set('PACKAGE_VERSION', meson.project_version())
  bin_conf.set('prefix', get_option('prefix'))
  bin_conf.set('libdir', join_paths(get_option('prefix'), get_option('libdir')))
  bin_conf.set('datadir', join_paths(get_option('prefix'), get_option('datadir')))

  configure_file(
    input: 'com.github.rafostar.Clapper' + id_postfix + '.in',
    output: 'com.github.rafostar.Clapper' + id_postfix,
    configuration: bin_conf,
    install: true,
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x'
  )
endforeach
