clappertube_dep = dependency('', required: false)
clappertube_option = get_option('clapper-tube')
clappertube_api_name = meson.project_name() + '-tube' + clapper_version_suffix
clappertube_plugins_dir = join_paths(prefix, libdir, clappertube_api_name)
build_clappertube = false

if clappertube_option.disabled()
  subdir_done()
endif

clappertube_deps = [
  glib_dep,
  gobject_dep,
  gst_dep,
  libsoup_dep,
]

foreach dep : clappertube_deps
  if not dep.found()
    if clappertube_option.enabled()
      error('clapper-tube option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

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
clappertube_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

clappertube_sources = [
  'clapper-tube-plugin.c',
  #'clapper-tube-src.c',
]
clappertube_c_args = [
  '-DG_LOG_DOMAIN="ClapperTube"',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  clappertube_c_args += ['-DGST_STATIC_COMPILATION']
endif

clappertube_lib = library(
  'gstclappertube',
  clappertube_sources,
  dependencies: clappertube_deps,
  include_directories: clappertube_conf_inc,
  c_args: clappertube_c_args,
  install: true,
  install_dir: gst_plugins_libdir,
)
build_clappertube = true

clappertube_dep = declare_dependency(
  link_with: clappertube_lib,
  include_directories: clappertube_conf_inc,
  dependencies: clappertube_deps,
)
