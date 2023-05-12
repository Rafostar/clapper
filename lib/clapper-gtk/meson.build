clappergtk_dep = dependency('', required: false)
clappergtk_option = get_option('libclappergtk')
clappergtk_api_name = meson.project_name() + '-gtk' + clapper_version_suffix
build_clappergtk = false

if clappergtk_option.disabled()
  subdir_done()
endif

clappergtk_deps = [
  clapper_dep,
  gst_dep,
  gst_clapper_sink_dep,
  gtk4_dep,
  glib_dep,
  gobject_dep,
]

foreach dep : clappergtk_deps
  if not dep.found()
    if clappergtk_option.enabled()
      error('libclappergtk option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

version_conf = configuration_data()

version_conf.set(
  'CLAPPER_GTK_VERSION',
  meson.project_version(),
)
version_conf.set(
  'CLAPPER_GTK_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'CLAPPER_GTK_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'CLAPPER_GTK_MICRO_VERSION',
  version_array[2].to_int(),
)

clappergtk_version_header = configure_file(
  input: 'clapper-gtk-version.h.in',
  output: 'clapper-gtk-version.h',
  configuration: version_conf,
)
clappergtk_resources = gnome.compile_resources(
  'clapper-gtk-resources',
  'clapper-gtk.gresources.xml',
  c_name: 'clapper_gtk',
)

# Include the generated headers
clappergtk_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

clappergtk_headers = [
  'clapper-gtk.h',
  'clapper-gtk-video-player.h',
  'clapper-gtk-simple-controls.h',
  'clapper-gtk-window-state-buttons.h',
  'clapper-gtk-container.h',
  'clapper-gtk-utils.h',
  clappergtk_version_header,
]
clappergtk_sources = [
  'clapper-gtk-video-player.c',
  'clapper-gtk-simple-controls.c',
  'clapper-gtk-window-state-buttons.c',
  'clapper-gtk-container.c',
  'clapper-gtk-limited-layout.c',
  'clapper-gtk-utils.c',
  clappergtk_resources,
]
clappergtk_c_args = [
  '-DG_LOG_DOMAIN="ClapperGtk"',
  '-DCLAPPER_GTK_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

clappergtk_headers_dir = join_paths(includedir, clapper_api_name, 'clapper-gtk')

clappergtk_enums = gnome.mkenums_simple(
  'clapper-gtk-enum-types',
  sources: clappergtk_headers,
  install_header: true,
  install_dir: clappergtk_headers_dir,
)

clappergtk_lib = library(
  clappergtk_api_name,
  clappergtk_sources + clappergtk_enums,
  dependencies: clappergtk_deps,
  include_directories: clappergtk_conf_inc,
  c_args: clappergtk_c_args,
  version: clapper_version,
  install: true,
)
install_headers(clappergtk_headers,
  install_dir: clappergtk_headers_dir,
)
build_clappergtk = true

if build_gir
  clappergtk_gir = gnome.generate_gir(clappergtk_lib,
    sources: [
      clappergtk_sources,
      clappergtk_headers,
      clappergtk_enums,
    ],
    extra_args: [
      gir_init_section,
      '--quiet',
      '--warn-all',
      '-DCLAPPER_GTK_COMPILATION',
      '-DGST_USE_UNSTABLE_API',
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'ClapperGtk',
    identifier_prefix: 'ClapperGtk',
    symbol_prefix: 'clapper_gtk',
    export_packages: clappergtk_api_name,
    install: true,
    includes: [
      clapper_gir[0],
      'Gst-1.0',
      'Gtk-4.0',
      'GLib-2.0',
      'GObject-2.0',
    ],
    header: join_paths(meson.project_name() + '-gtk', 'clapper-gtk.h'),
  )
endif

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('Cannot build "vapi" without "introspection"')
    endif
  else
    gnome.generate_vapi(clappergtk_api_name,
      sources: clappergtk_gir[0],
      packages: clapper_pkg_reqs + [
        clapper_vapi,
        'gtk4',
      ],
      install: true,
    )
  endif
endif

pkgconfig.generate(clappergtk_lib,
  subdirs: [clappergtk_api_name],
  filebase: clappergtk_api_name,
  name: meson.project_name() + '-gtk',
  version: meson.project_version(),
  description: 'Clapper GTK integration library',
  requires: [
    clapper_lib,
    'gstreamer-1.0',
    'gtk4',
    'glib-2.0',
    'gobject-2.0',
  ],
)

clappergtk_dep = declare_dependency(
  link_with: clappergtk_lib,
  include_directories: clappergtk_conf_inc,
  dependencies: clappergtk_deps,
  sources: [
    clappergtk_version_header,
    clappergtk_enums[1],
  ],
)
