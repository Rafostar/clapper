all_importers = [
  'glimporter',
  'gluploader',
  'rawimporter',
]

# We cannot build any importers without sink that they depend on
if not gst_clapper_sink_dep.found()
  foreach imp : all_importers
    if get_option(imp).enabled()
      error('"@0@" option was enabled, but it requires building gstreamer plugin'.format(imp))
    endif
  endforeach
endif

build_glimporter = (
  not get_option('glimporter').disabled()
  and gst_clapper_gl_ch_dep.found()
)

if build_glimporter
  library(
    'gstclapperglimporter',
    'gstclapperglimporter.c',
    dependencies: gst_clapper_gl_ch_dep,
    include_directories: gst_plugin_conf_inc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif

build_gluploader = (
  not get_option('gluploader').disabled()
  and gst_clapper_gl_ch_dep.found()
)

if build_gluploader
  library(
    'gstclappergluploader',
    'gstclappergluploader.c',
    dependencies: gst_clapper_gl_ch_dep,
    include_directories: gst_plugin_conf_inc,
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
    include_directories: gst_plugin_conf_inc,
    c_args: gst_clapper_plugin_args,
    install: true,
    install_dir: gst_clapper_importers_libdir,
  )
endif
