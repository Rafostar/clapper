gst_clapper_sink_dep = dependency('', required: false)

gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')
gst_clapper_importers_libdir = join_paths(clapper_libdir, 'gst', 'plugin', 'importers')

gst_clapper_plugin_deps = [
  gtk4_dep,
  gst_dep,
  gst_base_dep,
  gst_video_dep,
  gmodule_dep,
]

build_gst_plugin = not get_option('gst-plugin').disabled()
foreach dep : gst_clapper_plugin_deps
  if not dep.found()
    if get_option('gst-plugin').enabled()
      error('GStreamer plugin was enabled, but required dependencies were not found')
    endif
    build_gst_plugin = false
  endif
endforeach

gst_clapper_plugin_args = [
  '-DHAVE_CONFIG_H',
  '-DCLAPPER_GST_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  gst_clapper_plugin_args += [
    '-DGST_STATIC_COMPILATION',
    '-DCLAPPER_GST_STATIC_COMPILATION',
  ]
endif

cdata = configuration_data()

cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())

cdata.set_quoted('GST_PACKAGE_NAME', 'gst-plugin-clapper')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/Rafostar/clapper')
cdata.set_quoted('GST_LICENSE', 'LGPL')

cdata.set_quoted('CLAPPER_SINK_IMPORTER_PATH', gst_clapper_importers_libdir)

configure_file(
  output: 'config.h',
  configuration: cdata,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'CLAPPER_API',
  'CLAPPER_GST',
)

clappergst_visibility_header = configure_file(
  input: '../../shared/clapper-api-visibility.h.in',
  output: 'clapper-gst-visibility.h',
  configuration: visibility_conf,
)

gst_plugin_conf_inc = [
  include_directories('.'),
  include_directories('..'),
  include_directories('../..'),
]

gst_clapper_plugin_sources = [
  'gstclappersink.c',
  'gstclapperpaintable.c',
  'gstgtkutils.c',
  'gstplugin.c',
  'gstclappercontexthandler.c',
  'gstclapperimporter.c',
  'gstclapperimporterloader.c',
]

if build_gst_plugin
  gst_clapper_sink_dep = declare_dependency(
    link_with: library('gstclapper',
      gst_clapper_plugin_sources,
      c_args: gst_clapper_plugin_args,
      include_directories: gst_plugin_conf_inc,
      dependencies: gst_clapper_plugin_deps,
      install: true,
      install_dir: gst_plugins_libdir,
    ),
    include_directories: gst_plugin_conf_inc,
    dependencies: gst_clapper_plugin_deps,
  )
endif

subdir('handlers')
subdir('importers')
