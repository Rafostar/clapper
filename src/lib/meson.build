gir = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = (gir.found() and not get_option('introspection').disabled())

vapigen = find_program('vapigen', required: get_option('vapi'))
build_vapi = (vapigen.found() and not get_option('vapi').disabled())

gir_init_section = '--add-init-section=extern void gst_init(gint*,gchar**);' + \
  'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
  'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
  'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
  'gst_init(NULL,NULL);'

subdir('gst')
subdir('clapper')
