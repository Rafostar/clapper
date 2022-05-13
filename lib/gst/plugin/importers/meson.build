gst_clapper_gl_base_importer_dep = dependency('', required: false)

all_importers = [
  'glimporter',
  'gluploader',
  'rawimporter',
]

build_glbase = (
  not get_option('glimporter').disabled()
  or not get_option('gluploader').disabled()
)
gl_support_required = (
  get_option('glimporter').enabled()
  or get_option('gluploader').enabled()
)

# We cannot build any importers without sink that they depend on
if not gst_clapper_sink_dep.found()
  foreach imp : all_importers
    if get_option(imp).enabled()
      error('"@0@" option was enabled, but it requires building gstreamer plugin'.format(imp))
    endif
  endforeach
endif

gst_plugin_gl_base_deps = [gst_clapper_sink_dep, gstgl_dep, gstglproto_dep]
have_gtk_gl_windowing = false

if gst_gl_have_window_x11 and (gst_gl_have_platform_egl or gst_gl_have_platform_glx)
  gtk_x11_dep = dependency('gtk4-x11', required: false)
  if gtk_x11_dep.found()
    gst_plugin_gl_base_deps += gtk_x11_dep
    if gst_gl_have_platform_glx
      gst_plugin_gl_base_deps += gstglx11_dep
    endif
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_wayland and gst_gl_have_platform_egl
  gtk_wayland_dep = dependency('gtk4-wayland', required: false)
  if gtk_wayland_dep.found()
    gst_plugin_gl_base_deps += [gtk_wayland_dep, gstglwayland_dep]
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_win32 and (gst_gl_have_platform_egl or gst_gl_have_platform_wgl)
  gtk_win32_dep = dependency('gtk4-win32', required: false)
  if gtk_win32_dep.found()
    gst_plugin_gl_base_deps += gtk_win32_dep
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_cocoa and gst_gl_have_platform_cgl
  gtk_macos_dep = dependency('gtk4-macos', required: false)
  if gtk_macos_dep.found()
    gst_plugin_gl_base_deps += gtk_macos_dep
    have_gtk_gl_windowing = true
  endif
endif

if not have_gtk_gl_windowing
  if gl_support_required
    error('GL-based importer was enabled, but support for current GL windowing is missing')
  endif
  build_glbase = false
endif

if gst_gl_have_platform_egl
  gst_plugin_gl_base_deps += gstglegl_dep
endif

foreach dep : gst_plugin_gl_base_deps
  if not dep.found()
    if gl_support_required
      error('GL-based importer was enabled, but required dependencies were not found')
    endif
    build_glbase = false
  endif
endforeach

if build_glbase
  gst_clapper_gl_base_importer_dep = declare_dependency(
    link_with: library('gstclapperglbaseimporter',
      'gstclapperglbaseimporter.c',
      c_args: gst_clapper_plugin_args,
      include_directories: configinc,
      dependencies: gst_plugin_gl_base_deps,
      version: libversion,
      install: true,
    ),
    include_directories: configinc,
    dependencies: gst_plugin_gl_base_deps,
  )
endif

build_glimporter = (
  not get_option('glimporter').disabled()
  and gst_clapper_gl_base_importer_dep.found()
)

if build_glimporter
  library(
    'gstclapperglimporter',
    'gstclapperglimporter.c',
    dependencies: gst_clapper_gl_base_importer_dep,
    include_directories: configinc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif

build_gluploader = (
  not get_option('gluploader').disabled()
  and gst_clapper_gl_base_importer_dep.found()
)

if build_gluploader
  library(
    'gstclappergluploader',
    'gstclappergluploader.c',
    dependencies: gst_clapper_gl_base_importer_dep,
    include_directories: configinc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif

# No need to auto build rawimporter if we are building gluploader
build_rawimporter = (
  not get_option('rawimporter').disabled()
  and (not build_gluploader or get_option('rawimporter').enabled())
  and gst_clapper_sink_dep.found()
)

if build_rawimporter
  library(
    'gstclapperrawimporter',
    'gstclapperrawimporter.c',
    dependencies: gst_clapper_sink_dep,
    include_directories: configinc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif
