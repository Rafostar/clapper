gst_clapper_gl_ch_dep = dependency('', required: false)

build_gl_ch = (
  not get_option('glimporter').disabled()
  or not get_option('gluploader').disabled()
)
gl_support_required = (
  get_option('glimporter').enabled()
  or get_option('gluploader').enabled()
)

gst_plugin_gl_ch_deps = [gst_clapper_sink_dep, gstgl_dep, gstglproto_dep]
have_gtk_gl_windowing = false

if gst_gl_have_window_x11 and (gst_gl_have_platform_egl or gst_gl_have_platform_glx)
  gtk_x11_dep = dependency('gtk4-x11', required: false)
  if gtk_x11_dep.found()
    gst_plugin_gl_ch_deps += gtk_x11_dep
    if gst_gl_have_platform_glx
      gst_plugin_gl_ch_deps += gstglx11_dep
    endif
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_wayland and gst_gl_have_platform_egl
  gtk_wayland_dep = dependency('gtk4-wayland', required: false)
  if gtk_wayland_dep.found()
    gst_plugin_gl_ch_deps += [gtk_wayland_dep, gstglwayland_dep]
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
  gst_plugin_gl_ch_deps += gstglegl_dep
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
      include_directories: configinc,
      dependencies: gst_plugin_gl_ch_deps,
      version: libversion,
      install: true,
    ),
    include_directories: configinc,
    dependencies: gst_plugin_gl_ch_deps,
  )
endif
