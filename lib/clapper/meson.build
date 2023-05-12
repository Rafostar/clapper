clapper_dep = dependency('', required: false)
clapper_option = get_option('libclapper')
built_clapper = false

if clapper_option.disabled()
  subdir_done()
endif

clapper_deps = [
  gst_dep,
  gst_base_dep,
  gst_video_dep,
  gst_audio_dep,
  gst_tag_dep,
  gst_pbutils_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
]

if os_unix
  clapper_deps += gio_unix_dep
else
  clapper_deps += gio_win_dep
endif

foreach dep : clapper_deps
  if not dep.found()
    if clapper_option.enabled()
      error('libclapper option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

version_conf = configuration_data()

version_conf.set(
  'CLAPPER_VERSION',
  meson.project_version(),
)
version_conf.set(
  'CLAPPER_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'CLAPPER_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'CLAPPER_MICRO_VERSION',
  version_array[2].to_int(),
)

clapper_headers_dir = join_paths(includedir, clapper_api_name, 'clapper')

clapper_version_header = configure_file(
  input: 'clapper-version.h.in',
  output: 'clapper-version.h',
  configuration: version_conf,
)

# Include the generated headers
clapper_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

clapper_headers = [
  'clapper.h',
  'clapper-enums.h',
  'clapper-discoverer.h',
  'clapper-player.h',
  'clapper-queue.h',
  'clapper-media-item.h',
  'clapper-feature.h',
  'clapper-threaded-object.h',
  clapper_version_header,
]
clapper_sources = [
  'clapper.c',
  'clapper-app-bus.c',
  'clapper-discoverer.c',
  'clapper-playbin-bus.c',
  'clapper-player.c',
  'clapper-queue.c',
  'clapper-media-item.c',
  'clapper-feature.c',
  'clapper-features-manager.c',
  'clapper-features-bus.c',
  'clapper-threaded-object.c',
  'clapper-utils.c',
]
clapper_c_args = [
  '-DG_LOG_DOMAIN="Clapper"',
  '-DCLAPPER_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

subdir('features')

clapper_enums = gnome.mkenums_simple(
  'clapper-enum-types',
  sources: clapper_headers,
  install_header: true,
  install_dir: clapper_headers_dir,
)

clapper_lib = library(
  clapper_api_name,
  clapper_sources + clapper_features_sources + clapper_enums,
  dependencies: clapper_deps,
  include_directories: clapper_conf_inc,
  c_args: clapper_c_args,
  version: clapper_version,
  install: true,
)
install_headers(clapper_headers,
  install_dir: clapper_headers_dir,
)
build_clapper = true

pkg_reqs = [
  'gstreamer-1.0',
  'gstreamer-base-1.0',
  'gstreamer-video-1.0',
  'gstreamer-audio-1.0',
  'gstreamer-tag-1.0',
  'gstreamer-pbutils-1.0',
  'glib-2.0',
  'gobject-2.0',
  'gio-2.0',
]

if os_unix
  pkg_reqs += 'gio-unix-2.0'
else
  pkg_reqs += 'gio-windows-2.0'
endif

gir = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = (gir.found() and not get_option('introspection').disabled())

gir_init_section = '--add-init-section=extern void gst_init(gint*,gchar**);' + \
    'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
    'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
    'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
    'gst_init(NULL,NULL);'

if build_gir
  clapper_gir = gnome.generate_gir(clapper_lib,
    sources: [
      clapper_sources,
      clapper_features_sources,
      clapper_headers,
      clapper_features_headers,
      clapper_enums,
    ],
    extra_args: [
      gir_init_section,
      '--quiet',
      '--warn-all',
      '-DCLAPPER_COMPILATION',
      '-DGST_USE_UNSTABLE_API',
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'Clapper',
    identifier_prefix: 'Clapper',
    symbol_prefix: 'clapper',
    export_packages: clapper_api_name,
    install: true,
    includes: [
      'Gst-1.0',
      'GstBase-1.0',
      'GstVideo-1.0',
      'GstAudio-1.0',
      'GstTag-1.0',
      'GstPbutils-1.0',
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
    ],
    header: join_paths(meson.project_name(), 'clapper.h'),
  )
endif

vapigen = find_program('vapigen', required: get_option('vapi'))
build_vapi = (vapigen.found() and not get_option('vapi').disabled())

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    gnome.generate_vapi(clapper_api_name,
      sources: clapper_gir[0],
      packages: pkg_reqs,
      install: true,
    )
  endif
endif

pkgconfig.generate(
  libraries: clapper_lib,
  subdirs: [clapper_api_name],
  filebase: clapper_api_name,
  name: meson.project_name(),
  version: meson.project_version(),
  description: 'Clapper playback library',
  requires: pkg_reqs,
)

clapper_dep = declare_dependency(
  link_with: clapper_lib,
  include_directories: clapper_conf_inc,
  dependencies: clapper_deps,
  sources: [
    clapper_version_header,
    clapper_features_availability_header,
    clapper_enums[1]
  ],
)
