gstclapper_sources = [
  'gstclapper.c',
  'gstclapper-signal-dispatcher.c',
  'gstclapper-video-renderer.c',
  'gstclapper-media-info.c',
  'gstclapper-g-main-context-signal-dispatcher.c',
  'gstclapper-video-overlay-video-renderer.c',
  'gstclapper-visualization.c',
  'gstclapper-mpris.c',
  'gstclapper-gtk4-plugin.c',

  'gtk4/gstclapperglsink.c',
  'gtk4/gstgtkutils.c',
  'gtk4/gtkclapperglwidget.c',
  'gtk4/gstclapperglutils.c',
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
  'gstclapper-mpris.h',
  'gstclapper-gtk4-plugin.h',
]
gstclapper_defines = [
  '-DHAVE_CONFIG_H',
  '-DBUILDING_GST_CLAPPER',
  '-DGST_USE_UNSTABLE_API',
  '-DHAVE_GTK_GL',
]

if not get_option('lib')
  subdir_done()
endif

if not gir.found()
  error('Clapper lib requires GI bindings to be compiled')
endif

if not have_gtk_gl_windowing
  error('GTK4 widget requires GL windowing')
endif

gstclapper_mpris_gdbus = gnome.gdbus_codegen('gstclapper-mpris-gdbus',
  sources: '../../../data/gstclapper-mpris-gdbus.xml',
  interface_prefix: 'org.mpris.',
  namespace: 'GstClapperMpris'
)

gstclapper = library('gstclapper-' + api_version,
  gstclapper_sources + gstclapper_mpris_gdbus,
  c_args: gstclapper_defines,
  link_args: noseh_link_args,
  include_directories: [configinc, libsinc],
  version: libversion,
  install: true,
  install_dir: pkglibdir,
  dependencies: [gtk4_dep, glib_dep, gio_dep, giounix_dep,
    gstbase_dep, gstvideo_dep, gstaudio_dep,
    gsttag_dep, gstpbutils_dep, libm] + gtk_deps,
)

clapper_gir = gnome.generate_gir(gstclapper,
  sources: gstclapper_sources + gstclapper_headers,
  namespace: 'GstClapper',
  nsversion: api_version,
  identifier_prefix: 'Gst',
  symbol_prefix: 'gst',
  export_packages: 'gstreamer-clapper-1.0',
  includes: ['Gst-1.0', 'GstPbutils-1.0', 'GstBase-1.0', 'GstVideo-1.0',
    'GstAudio-1.0', 'GstTag-1.0'],
  install: true,
  install_dir_typelib: join_paths(pkglibdir, 'girepository-1.0'),
  extra_args: gir_init_section + ['-DGST_USE_UNSTABLE_API'],
  dependencies: [gstbase_dep, gstvideo_dep, gstaudio_dep,
    gsttag_dep, gstpbutils_dep]
)
