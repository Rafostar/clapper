clapper_dep = dependency('', required: false)
clapper_option = get_option('clapper')
clapper_headers_dir = join_paths(includedir, clapper_api_name, 'clapper')
clapper_enhancers_dir = join_paths(clapper_libdir, 'enhancers')
clapper_with_enhancers_loader = false
build_clapper = false

clapper_pkg_reqs = [
  'gstreamer-1.0',
  'gstreamer-base-1.0',
  'gstreamer-audio-1.0',
  'gstreamer-tag-1.0',
  'gstreamer-pbutils-1.0',
  'glib-2.0',
  'gobject-2.0',
  'gio-2.0',
]

if clapper_option.disabled()
  subdir_done()
endif

clapper_deps = [
  gst_dep,
  gst_base_dep,
  gst_audio_dep,
  gst_tag_dep,
  gst_pbutils_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
]

foreach dep : clapper_deps
  if not dep.found()
    if clapper_option.enabled()
      error('clapper option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

# libpeas is an optional dependency
enhancers_option = get_option('enhancers-loader')
clapper_with_enhancers_loader = (not enhancers_option.disabled() and peas_dep.found())

if not clapper_with_enhancers_loader and enhancers_option.enabled()
  error('enhancers-loader option was enabled, but required dependencies were not found')
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_ORIGIN', 'https://github.com/Rafostar/clapper')
config_h.set_quoted('PLUGIN_DESC', 'Clapper elements')
config_h.set_quoted('PLUGIN_LICENSE', 'LGPL')
config_h.set_quoted('CLAPPER_ENHANCERS_PATH', clapper_enhancers_dir)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'CLAPPER_API',
  'CLAPPER',
)

clapper_visibility_header = configure_file(
  input: '../shared/clapper-api-visibility.h.in',
  output: 'clapper-visibility.h',
  configuration: visibility_conf,
)

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
  'clapper-audio-stream.h',
  'clapper-extractable.h',
  'clapper-feature.h',
  'clapper-harvest.h',
  'clapper-marker.h',
  'clapper-media-item.h',
  'clapper-player.h',
  'clapper-queue.h',
  'clapper-stream.h',
  'clapper-stream-list.h',
  'clapper-subtitle-stream.h',
  'clapper-threaded-object.h',
  'clapper-timeline.h',
  'clapper-utils.h',
  'clapper-video-stream.h',
  clapper_version_header,
  clapper_visibility_header,
]
clapper_sources = [
  'clapper.c',
  'clapper-app-bus.c',
  'clapper-audio-stream.c',
  'clapper-extractable.c',
  'clapper-feature.c',
  'clapper-features-bus.c',
  'clapper-features-manager.c',
  'clapper-harvest.c',
  'clapper-marker.c',
  'clapper-media-item.c',
  'clapper-playbin-bus.c',
  'clapper-player.c',
  'clapper-queue.c',
  'clapper-stream.c',
  'clapper-stream-list.c',
  'clapper-subtitle-stream.c',
  'clapper-threaded-object.c',
  'clapper-timeline.c',
  'clapper-utils.c',
  'clapper-video-stream.c',
  'gst/clapper-plugin.c',
  'gst/clapper-uri-list-demux.c',
  '../shared/clapper-shared-utils.c',
]
clapper_c_args = [
  '-DG_LOG_DOMAIN="Clapper"',
  '-DCLAPPER_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  clapper_c_args += ['-DCLAPPER_STATIC_COMPILATION']
endif

clapper_possible_functionalities = [
  'enhancers-loader',
]
clapper_available_functionalities = []

if clapper_with_enhancers_loader
  clapper_deps += peas_dep
  clapper_sources += [
    'clapper-enhancers-loader.c',
    'gst/clapper-enhancer-src.c',
    'gst/clapper-enhancer-director.c',
  ]
  clapper_available_functionalities += 'enhancers-loader'
endif

functionalities_availability_conf = configuration_data()

foreach functionality_name : clapper_possible_functionalities
  functionalities_availability_conf.set(
    'CLAPPER_WITH_@0@'.format(functionality_name.replace('-', '_').to_upper()),
    clapper_available_functionalities.contains(functionality_name) ? 'TRUE' : 'FALSE'
  )
endforeach

clapper_headers += configure_file(
  input: 'clapper-functionalities-availability.h.in',
  output: 'clapper-functionalities-availability.h',
  configuration: functionalities_availability_conf,
)

subdir('features')

clapper_enums = gnome.mkenums_simple(
  'clapper-enum-types',
  sources: clapper_headers,
  decorator: 'CLAPPER_API',
  header_prefix: '#include <clapper/clapper-visibility.h>',
  identifier_prefix: 'Clapper',
  symbol_prefix: 'clapper',
  install_header: true,
  install_dir: clapper_headers_dir,
)

clapper_lib = library(
  clapper_api_name,
  clapper_sources + clapper_features_sources + clapper_features_sources_internal + clapper_enums,
  dependencies: clapper_deps + clapper_features_deps,
  include_directories: clapper_conf_inc,
  c_args: clapper_c_args,
  version: clapper_version,
  install: true,
)
install_headers(clapper_headers,
  install_dir: clapper_headers_dir,
)
build_clapper = true

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

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    clapper_vapi = gnome.generate_vapi(clapper_api_name,
      sources: clapper_gir[0],
      packages: clapper_pkg_reqs,
      metadata_dirs: [
        join_paths(meson.current_source_dir(), 'metadata')
      ],
      install: true,
    )
  endif
endif

clapper_pkgconfig_variables = [
  'enhancersdir=${libdir}/' + clapper_api_name + '/enhancers',
  'features=' + ' '.join(clapper_available_features),
  'functionalities=' + ' '.join(clapper_available_functionalities),
]

pkgconfig.generate(clapper_lib,
  unescaped_variables: clapper_pkgconfig_variables,
  subdirs: [clapper_api_name],
  filebase: clapper_api_name,
  name: meson.project_name(),
  version: meson.project_version(),
  description: 'Clapper playback library',
  requires: clapper_pkg_reqs,
)

clapper_dep = declare_dependency(
  link_with: clapper_lib,
  include_directories: clapper_conf_inc,
  dependencies: clapper_deps + clapper_features_deps,
  sources: [
    clapper_visibility_header,
    clapper_version_header,
    clapper_features_availability_header,
    clapper_enums[1],
  ],
)
