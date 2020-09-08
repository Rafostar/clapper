importspath = get_option('sharedir').split('/').join('.') + '[\'' + meson.project_name() + '\']'

gjs_conf = configuration_data()
gjs_conf.set('importspath', importspath)

configure_file(
  input: 'clapper.js.in',
  output: 'clapper.js',
  configuration: gjs_conf,
  install: true,
  install_dir: get_option('gjsdir')
)
