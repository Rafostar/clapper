gstclapper_sources = [
  'gstclapper.c',
  'gstclapper-signal-dispatcher.c',
  'gstclapper-video-renderer.c',
  'gstclapper-media-info.c',
  'gstclapper-g-main-context-signal-dispatcher.c',
  'gstclapper-video-overlay-video-renderer.c',
  'gstclapper-visualization.c',
  'gstclapper-gtk4-plugin.c',

  'gtk4/gstgtkbasesink.c',
  'gtk4/gstgtkutils.c',
  'gtk4/gtkgstbasewidget.c',
  'gtk4/gstgtkglsink.c',
  'gtk4/gtkgstglwidget.c',
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
  'gstclapper-gtk4-plugin.h',
]
gstclapper_defines = [
  '-DHAVE_CONFIG_H',
  '-DBUILDING_GST_CLAPPER',
  '-DGST_USE_UNSTABLE_API',
  '-DHAVE_GTK_GL',
  '-DBUILD_FOR_GTK4',
]
gtk_deps = [gstgl_dep, gstglproto_dep]
have_gtk_gl_windowing = false

gtk4_dep = dependency('gtk4', required : true)

if not gtk4_dep.found() or not gtk4_dep.version().version_compare('>=4.0.0')
  error('GTK4 is missing or is too old')
endif

if not have_gstgl
  error('GstGL is missing')
endif

if gst_gl_have_window_x11 and gst_gl_have_platform_glx
  gtk_x11_dep = dependency('gtk4-x11', required : false)
  if gtk_x11_dep.found()
    gtk_deps += [gtk_x11_dep, gstglx11_dep]
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_wayland and gst_gl_have_platform_egl
  gtk_wayland_dep = dependency('gtk4-wayland', required : false)
  if gtk_wayland_dep.found()
    gtk_deps += [gtk_wayland_dep, gstglegl_dep, gstglwayland_dep]
    have_gtk_gl_windowing = true
  endif
endif

if not have_gtk_gl_windowing
  error('GTK4 widget requires GL windowing')
endif

gstclapper = library('gstclapper-' + api_version,
  gstclapper_sources,
  c_args : gstclapper_defines,
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  version : libversion,
  install : true,
  dependencies : [gtk4_dep, gstbase_dep, gstvideo_dep, gstaudio_dep,
    gsttag_dep, gstpbutils_dep, libm] + gtk_deps,
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