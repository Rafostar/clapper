gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')

gst_clapper_plugin_args = [
  '-DHAVE_CONFIG_H',
  '-DGST_USE_UNSTABLE_API',
]

gst_clapper_sink_dep = dependency('', required: false)
gtk4_dep = dependency('gtk4', version: '>=4.6.0', required: false)

gmodule_dep = dependency('gmodule-2.0',
  version: glib_req,
  required: false,
  fallback: ['glib', 'libgmodule_dep'],
)

gst_clapper_plugin_deps = [
  gtk4_dep,
  gst_dep,
  gstbase_dep,
  gstvideo_dep,
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

if get_option('default_library') == 'static'
  gst_clapper_plugin_args += ['-DGST_STATIC_COMPILATION']
endif

gst_clapper_plugin_sources = [
  'gstclappersink.c',
  'gstclapperpaintable.c',
  'gstgtkutils.c',
  'gstplugin.c',
  'gstclapperimporter.c',
  'gstclapperimporterloader.c',
]

if build_gst_plugin
  gst_clapper_sink_dep = declare_dependency(
    link_with: library('gstclapper',
      gst_clapper_plugin_sources,
      c_args: gst_clapper_plugin_args,
      include_directories: configinc,
      dependencies: gst_clapper_plugin_deps,
      install: true,
      install_dir: gst_plugins_libdir,
    ),
    include_directories: configinc,
    dependencies: gst_clapper_plugin_deps,
  )
endif

subdir('importers')
