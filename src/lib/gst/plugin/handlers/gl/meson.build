gst_clapper_gl_ch_dep = dependency('', required: false)

build_gl_ch = (
  not get_option('glimporter').disabled()
  or not get_option('gluploader').disabled()
)
gl_support_required = (
  get_option('glimporter').enabled()
  or get_option('gluploader').enabled()
)

# GStreamer OpenGL
gst_gl_dep = dependency('gstreamer-gl-1.0',
  version: gst_req,
  fallback: ['gst-plugins-base'],
  required: false,
)
gst_gl_x11_dep = dependency('', required: false)
gst_gl_wayland_dep = dependency('', required: false)
gst_gl_egl_dep = dependency('', required: false)

gst_gl_apis = gst_gl_dep.get_variable('gl_apis').split()
gst_gl_winsys = gst_gl_dep.get_variable('gl_winsys').split()
gst_gl_platforms = gst_gl_dep.get_variable('gl_platforms').split()

message('GStreamer OpenGL window systems: @0@'.format(' '.join(gst_gl_winsys)))
message('GStreamer OpenGL platforms: @0@'.format(' '.join(gst_gl_platforms)))
message('GStreamer OpenGL apis: @0@'.format(' '.join(gst_gl_apis)))

foreach ws : ['x11', 'wayland', 'android', 'cocoa', 'eagl', 'win32', 'dispmanx', 'viv_fb']
  set_variable('gst_gl_have_window_@0@'.format(ws), gst_gl_winsys.contains(ws))
endforeach

foreach p : ['glx', 'egl', 'cgl', 'eagl', 'wgl']
  set_variable('gst_gl_have_platform_@0@'.format(p), gst_gl_platforms.contains(p))
endforeach

foreach api : ['gl', 'gles2']
  set_variable('gst_gl_have_api_@0@'.format(api), gst_gl_apis.contains(api))
endforeach

gst_gl_proto_dep = dependency('gstreamer-gl-prototypes-1.0',
  version: gst_req,
  fallback: ['gst-plugins-base'],
  required: true
)
if gst_gl_have_window_x11
  gst_gl_x11_dep = dependency('gstreamer-gl-x11-1.0',
    version: gst_req,
    fallback: ['gst-plugins-base'],
    required: true,
)
endif
if gst_gl_have_window_wayland
  gst_gl_wayland_dep = dependency('gstreamer-gl-wayland-1.0',
    version: gst_req,
    fallback: ['gst-plugins-base'],
    required: true,
)
endif
if gst_gl_have_platform_egl
  gst_gl_egl_dep = dependency('gstreamer-gl-egl-1.0',
    version: gst_req,
    fallback: ['gst-plugins-base'],
    required: true,
)
endif

gst_plugin_gl_ch_deps = [gst_clapper_sink_dep, gst_gl_dep, gst_gl_proto_dep]
have_gtk_gl_windowing = false

if gst_gl_have_window_x11 and (gst_gl_have_platform_egl or gst_gl_have_platform_glx)
  gtk_x11_dep = dependency('gtk4-x11', required: false)
  if gtk_x11_dep.found()
    gst_plugin_gl_ch_deps += gtk_x11_dep
    if gst_gl_have_platform_glx
      gst_plugin_gl_ch_deps += gst_gl_x11_dep
    endif
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_wayland and gst_gl_have_platform_egl
  gtk_wayland_dep = dependency('gtk4-wayland', required: false)
  if gtk_wayland_dep.found()
    gst_plugin_gl_ch_deps += [gtk_wayland_dep, gst_gl_wayland_dep]
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_win32 and (gst_gl_have_platform_egl or gst_gl_have_platform_wgl)
  gtk_win32_dep = dependency('gtk4-win32', required: false)
  if gtk_win32_dep.found()
    gst_plugin_gl_ch_deps += gtk_win32_dep
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_cocoa and gst_gl_have_platform_cgl
  gtk_macos_dep = dependency('gtk4-macos', required: false)
  if gtk_macos_dep.found()
    gst_plugin_gl_ch_deps += gtk_macos_dep
    have_gtk_gl_windowing = true
  endif
endif

if not have_gtk_gl_windowing
  if gl_support_required
    error('GL-based importer was enabled, but support for current GL windowing is missing')
  endif
  build_gl_ch = false
endif

if gst_gl_have_platform_egl
  gst_plugin_gl_ch_deps += gst_gl_egl_dep
endif

foreach dep : gst_plugin_gl_ch_deps
  if not dep.found()
    if gl_support_required
      error('GL-based importer was enabled, but required dependencies were not found')
    endif
    build_gl_ch = false
  endif
endforeach

if build_gl_ch
  gst_clapper_gl_ch_dep = declare_dependency(
    link_with: library('gstclapperglcontexthandler',
      'gstclapperglcontexthandler.c',
      c_args: gst_clapper_plugin_args,
      include_directories: gst_plugin_conf_inc,
      dependencies: gst_plugin_gl_ch_deps,
      version: meson.project_version(),
      install: true,
    ),
    include_directories: gst_plugin_conf_inc,
    dependencies: gst_plugin_gl_ch_deps,
  )
endif
