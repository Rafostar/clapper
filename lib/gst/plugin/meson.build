gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')

gst_clapper_plugin_args = [
  '-DHAVE_CONFIG_H',
  '-DGST_USE_UNSTABLE_API',
]
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

foreach dep : gst_clapper_plugin_deps
  if not dep.found()
    if get_option('gst-plugin').enabled()
      error('GStreamer plugin was enabled, but required dependencies were not found')
    endif
    subdir_done()
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

subdir('importers')
