gstclapper_sources = [
  'gstclapper.c',
  'gstclapper-signal-dispatcher.c',
  'gstclapper-video-renderer.c',
  'gstclapper-media-info.c',
  'gstclapper-g-main-context-signal-dispatcher.c',
  'gstclapper-video-overlay-video-renderer.c',
  'gstclapper-visualization.c',
]

gstclapper_headers = [
  'clapper.h',
  'clapper-prelude.h',
  'gstclapper.h',
  'gstclapper-types.h',
  'gstclapper-signal-dispatcher.h',
  'gstclapper-video-renderer.h',
  'gstclapper-media-info.h',
  'gstclapper-g-main-context-signal-dispatcher.h',
  'gstclapper-video-overlay-video-renderer.h',
  'gstclapper-visualization.h',
]

if not build_gir
  error('Clapper requires GI bindings to be compiled')
endif

gstclapper = library('gstclapper-' + api_version,
  gstclapper_sources,
  c_args : gst_clapper_args + ['-DBUILDING_GST_CLAPPER'],
  include_directories : [configinc, libsinc],
  version : libversion,
  install : true,
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep,
    gsttag_dep, gstpbutils_dep],
)

clapper_gir = gnome.generate_gir(gstclapper,
  sources : gstclapper_sources + gstclapper_headers,
  namespace : 'GstClapper',
  nsversion : api_version,
  identifier_prefix : 'Gst',
  symbol_prefix : 'gst',
  export_packages : 'gstreamer-clapper-1.0',
  includes : ['Gst-1.0', 'GstPbutils-1.0', 'GstBase-1.0', 'GstVideo-1.0',
    'GstAudio-1.0', 'GstTag-1.0'],
  install : true,
  extra_args : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
  dependencies : [gstbase_dep, gstvideo_dep, gstaudio_dep,
    gsttag_dep, gstpbutils_dep]
)
