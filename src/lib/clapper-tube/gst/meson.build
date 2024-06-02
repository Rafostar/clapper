config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_ORIGIN', 'https://github.com/Rafostar/clapper')
config_h.set_quoted('PLUGIN_DESC', 'Clapper Tube elements')
config_h.set_quoted('PLUGIN_LICENSE', 'LGPL')
config_h.set_quoted('CLAPPER_TUBE_PLUGIN_PATH', clappertube_plugins_dir)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

# Include the generated headers
gstclappertube_conf_inc = [
  include_directories('.'),
]

gstclappertube_sources = [
  'clapper-tube-plugin.c',
  #'clapper-tube-src.c',
]
gstclappertube_c_args = [
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  gstclappertube_c_args += ['-DGST_STATIC_COMPILATION']
endif

gstclappertube_lib = library(
  'gstclappertube',
  gstclappertube_sources,
  dependencies: gstclappertube_deps,
  include_directories: gstclappertube_conf_inc,
  c_args: gstclappertube_c_args,
  install: true,
  install_dir: gst_plugins_libdir,
)
