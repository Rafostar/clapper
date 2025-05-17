gir = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = (gir.found() and not get_option('introspection').disabled())

vapigen = find_program('vapigen', required: get_option('vapi'))
build_vapi = (vapigen.found() and not get_option('vapi').disabled())

subdir('gst')
subdir('clapper')
subdir('clapper-gtk')
