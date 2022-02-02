gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')

gst_clapper_plugin_args = [
  '-DHAVE_CONFIG_H',
  '-DGST_USE_UNSTABLE_API',
]
gst_clapper_plugin_deps = [
  gtk4_dep,
  gst_dep,
  gstbase_dep,
  gstvideo_dep,
  gstallocators_dep,
]

if get_option('default_library') == 'static'
  gst_clapper_plugin_args += ['-DGST_STATIC_COMPILATION']
endif

gst_clapper_plugin_option = get_option('gst-plugin')
if gst_clapper_plugin_option.disabled()
  subdir_done()
endif

foreach dep : gst_clapper_plugin_deps
  if not dep.found()
    if gst_clapper_plugin_option.enabled()
      error('GStreamer plugin was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

if not have_gtk_gl_windowing
  if gst_clapper_plugin_option.enabled()
    error('GTK4 widget requires GL windowing')
  else
    subdir_done()
  endif
endif

gst_clapper_plugin_sources = [
  'gstclappersink.c',
  'gtkclapperobject.c',
  'gstgtkutils.c',
  'gstplugin.c',
]

library('gstclapper',
  gst_clapper_plugin_sources,
  c_args: gst_clapper_plugin_args,
  include_directories: configinc,
  dependencies: gst_clapper_plugin_deps + gtk_deps,
  install: true,
  install_dir: gst_plugins_libdir,
)
