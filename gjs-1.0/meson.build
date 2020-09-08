gjs_conf = configuration_data()
gjs_conf.set('importsdir', join_paths(get_option('datadir'), meson.project_name()))

configure_file(
  input: 'clapper.js.in',
  output: 'clapper.js',
  configuration: gjs_conf,
  install: true,
  install_dir: get_option('gjsdir')
)
