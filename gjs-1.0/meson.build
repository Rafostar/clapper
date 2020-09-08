sharedir = join_paths(get_option('prefix'), 'share')
gjsdir = join_paths(sharedir, 'gjs-1.0')
importspath = join_paths(sharedir, meson.project_name())

gjs_conf = configuration_data()
gjs_conf.set('importspath', importspath)

configure_file(
  input: 'clapper.js.in',
  output: 'clapper.js',
  configuration: gjs_conf,
  install: true,
  install_dir: gjsdir
)
