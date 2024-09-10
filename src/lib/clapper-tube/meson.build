clappertube_dep = dependency('', required: false)
clappertube_option = get_option('clapper-tube')
clappertube_api_name = meson.project_name() + '-tube' + clapper_version_suffix
clappertube_plugins_dir = join_paths(prefix, libdir, clappertube_api_name)
build_clappertube = false

if clappertube_option.disabled()
  subdir_done()
endif

clappertube_deps = [
  clapper_dep,
  glib_dep,
  gobject_dep,
  gio_dep,
  libsoup_dep,
  gst_dep,
]
gstclappertube_deps = [
  glib_dep,
  gobject_dep,
  gst_dep,
  gst_base_dep,
]

# We either compile both or none with a single meson option
foreach deps : [clappertube_deps, gstclappertube_deps]
  foreach dep : deps
    if not dep.found()
      if clappertube_option.enabled()
        error('clapper-tube option was enabled, but required dependencies were not found')
      endif
      subdir_done()
    endif
  endforeach
endforeach

config_h = configuration_data()
config_h.set_quoted('CLAPPER_TUBE_API_NAME', clappertube_api_name)
config_h.set_quoted('CLAPPER_TUBE_PLUGIN_PATH', clappertube_plugins_dir)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'CLAPPER_API',
  'CLAPPER_TUBE',
)

clappertube_visibility_header = configure_file(
  input: '../shared/clapper-api-visibility.h.in',
  output: 'clapper-tube-visibility.h',
  configuration: visibility_conf,
)

internal_visibility_conf = configuration_data()

internal_visibility_conf.set(
  'CLAPPER_API',
  'CLAPPER_TUBE_INTERNAL',
)

clappertube_internal_visibility_header = configure_file(
  input: '../shared/clapper-api-visibility.h.in',
  output: 'clapper-tube-internal-visibility.h',
  configuration: internal_visibility_conf,
)

version_conf = configuration_data()

version_conf.set(
  'CLAPPER_TUBE_VERSION',
  meson.project_version(),
)
version_conf.set(
  'CLAPPER_TUBE_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'CLAPPER_TUBE_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'CLAPPER_TUBE_MICRO_VERSION',
  version_array[2].to_int(),
)

clappertube_version_header = configure_file(
  input: 'clapper-tube-version.h.in',
  output: 'clapper-tube-version.h',
  configuration: version_conf,
)

# Include the generated headers
clappertube_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

clappertube_headers = [
  'clapper-tube.h',
  'clapper-tube-enums.h',
  'clapper-tube-cache.h',
  'clapper-tube-config.h',
  'clapper-tube-extractor.h',
  'clapper-tube-harvest.h',
  'clapper-tube-web-extractor.h',
  clappertube_version_header,
  clappertube_visibility_header,
]
clappertube_sources = [
  'clapper-tube.c',
  'clapper-tube-cache.c',
  'clapper-tube-client.c',
  'clapper-tube-config.c',
  'clapper-tube-extractor.c',
  'clapper-tube-harvest.c',
  'clapper-tube-loader.c',
  'clapper-tube-web-extractor.c',
  '../shared/clapper-shared-utils.c',
]
clappertube_c_args = [
  '-DG_LOG_DOMAIN="ClapperTube"',
  '-DCLAPPER_TUBE_COMPILATION',
  '-DCLAPPER_TUBE_INTERNAL_COMPILATION',
]

if get_option('default_library') == 'static'
  clappertube_c_args += ['-DCLAPPER_TUBE_STATIC_COMPILATION']
endif

clappertube_headers_dir = join_paths(includedir, clapper_api_name, 'clapper-tube')

clappertube_enums = gnome.mkenums_simple(
  'clapper-tube-enum-types',
  sources: clappertube_headers,
  decorator: 'CLAPPER_TUBE_API',
  header_prefix: '#include <clapper-tube/clapper-tube-visibility.h>',
  identifier_prefix: 'ClapperTube',
  symbol_prefix: 'clapper_tube',
  install_header: true,
  install_dir: clappertube_headers_dir,
)

clappertube_lib = library(
  clappertube_api_name,
  clappertube_sources + clappertube_enums,
  dependencies: clappertube_deps,
  include_directories: clappertube_conf_inc,
  c_args: clappertube_c_args,
  version: clapper_version,
  install: true,
)
install_headers(clappertube_headers,
  install_dir: clappertube_headers_dir,
)
build_clappertube = true

if build_gir
  clappertube_gir = gnome.generate_gir(clappertube_lib,
    sources: [
      clappertube_sources,
      clappertube_headers,
      clappertube_enums,
    ],
    extra_args: [
      gir_init_section,
      '--quiet',
      '--warn-all',
      '-DCLAPPER_TUBE_COMPILATION',
      '-DGST_USE_UNSTABLE_API',
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'ClapperTube',
    identifier_prefix: 'ClapperTube',
    symbol_prefix: 'clapper_tube',
    export_packages: clappertube_api_name,
    install: true,
    includes: [
      clapper_gir[0],
      'GLib-2.0',
      'GObject-2.0',
      'Gio-2.0',
      'Soup-3.0',
      'Gst-1.0',
      'GstTag-1.0',
    ],
    header: join_paths(meson.project_name() + '-tube', 'clapper-tube.h'),
  )
endif

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    gnome.generate_vapi(clappertube_api_name,
      sources: clappertube_gir[0],
      packages: clapper_pkg_reqs + [
        clapper_vapi,
        'libsoup-3.0',
      ],
      install: true,
    )
  endif
endif

pkgconfig.generate(clappertube_lib,
  subdirs: [clappertube_api_name],
  filebase: clappertube_api_name,
  name: meson.project_name() + '-tube',
  version: meson.project_version(),
  description: 'Clapper Tube library',
  requires: [
    clapper_lib,
    'glib-2.0',
    'gobject-2.0',
    'gio-2.0',
    'libsoup-3.0',
    'gstreamer-1.0',
    'gstreamer-base-1.0',
    'gstreamer-tag-1.0',
  ],
)

clappertube_dep = declare_dependency(
  link_with: clappertube_lib,
  include_directories: clappertube_conf_inc,
  dependencies: clappertube_deps,
  sources: [
    clappertube_visibility_header,
    clappertube_internal_visibility_header,
    clappertube_version_header,
    clappertube_enums[1],
  ],
)

gstclappertube_deps += clappertube_dep
subdir('gst')
