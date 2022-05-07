gst_clapper_gl_base_importer_dep = dependency('', required: false)

plugin_needs_gl_base = (
  not get_option('glimporter').disabled()
  or not get_option('gluploader').disabled()
  or not get_option('vaglimporter').disabled()
)
plugin_gl_support_required = (
  get_option('glimporter').enabled()
  or get_option('gluploader').enabled()
  or get_option('vaglimporter').enabled()
)

gst_plugin_gl_deps = [gstgl_dep, gstglproto_dep]
have_gtk_gl_windowing = false

if gst_gl_have_window_x11 and (gst_gl_have_platform_egl or gst_gl_have_platform_glx)
  gtk_x11_dep = dependency('gtk4-x11', required: false)
  if gtk_x11_dep.found()
    gst_plugin_gl_deps += gtk_x11_dep
    if gst_gl_have_platform_glx
      gst_plugin_gl_deps += gstglx11_dep
    endif
    have_gtk_gl_windowing = true
  endif
endif

if gst_gl_have_window_wayland and gst_gl_have_platform_egl
  gtk_wayland_dep = dependency('gtk4-wayland', required: false)
  if gtk_wayland_dep.found()
    gst_plugin_gl_deps += [gtk_wayland_dep, gstglwayland_dep]
    have_gtk_gl_windowing = true
  endif
endif

if plugin_gl_support_required and not have_gtk_gl_windowing
  error('GL-based importer was enabled, but support for current GL windowing is missing')
endif

if gst_gl_have_platform_egl
  gst_plugin_gl_deps += gstglegl_dep
endif

gst_clapper_gl_base_importer_deps = [
  gst_clapper_sink_dep
] + gst_plugin_gl_deps

foreach dep : gst_clapper_gl_base_importer_deps
  if not dep.found()
    if plugin_gl_support_required
      error('GL-based importer was enabled, but required dependencies were not found')
    endif
    plugin_needs_gl_base = false
  endif
endforeach

if plugin_needs_gl_base
  gst_clapper_gl_base_importer_dep = declare_dependency(
    link_with: library('gstclapperglbaseimporter',
      'gstclapperglbaseimporter.c',
      c_args: gst_clapper_plugin_args,
      include_directories: configinc,
      dependencies: gst_clapper_gl_base_importer_deps,
      version: libversion,
      install: true,
    ),
    include_directories: configinc,
    dependencies: gst_clapper_gl_base_importer_deps,
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

gst_va_dep = dependency('gstreamer-va-1.0',
  version: gst_req,
  required: false,
)

gst_clapper_va_gl_importer_deps = [
  gst_clapper_gl_base_importer_dep,
  gst_va_dep,
]

foreach dep : gst_clapper_va_gl_importer_deps
  if not dep.found()
    if get_option('vaglimporter').enabled()
      error('VA GL-based importer was enabled, but required dependencies were not found')
    endif
  endif
endforeach

build_vaglimporter = (
  not get_option('vaglimporter').disabled()
  and gst_clapper_gl_base_importer_dep.found()
)

if build_vaglimporter
  library(
    'gstclappervaglimporter',
    'gstclappervaglimporter.c',
    dependencies: gst_clapper_va_gl_importer_deps,
    include_directories: configinc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif
