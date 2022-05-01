if not get_option('rawimporter').disabled()
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
