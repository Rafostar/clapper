bin_conf = configuration_data()

bin_conf.set('GJS', find_program('gjs').path())
bin_conf.set('PACKAGE_NAME', meson.project_name())
bin_conf.set('PACKAGE_VERSION', meson.project_version())
bin_conf.set('prefix', get_option('prefix'))
bin_conf.set('libdir', libdir)

configure_file(
  input: 'com.github.rafostar.Clapper.in',
  output: 'com.github.rafostar.Clapper',
  configuration: bin_conf,
  install: true,
  install_dir: bindir,
  install_mode: 'rwxr-xr-x'
)

clapper_symlink_cmd = 'ln -fs @0@ $DESTDIR@1@'.format(
  'com.github.rafostar.Clapper',
  join_paths(bindir, 'clapper')
)
meson.add_install_script('sh', '-c', clapper_symlink_cmd)
