clappergtk_dep = dependency('', required: false)
clappergtk_option = get_option('clapper-gtk')
clappergtk_api_name = meson.project_name() + '-gtk' + clapper_version_suffix
clappergtk_resource_prefix = '/com/github/rafostar/Clapper/clapper-gtk'
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
  libm,
]

foreach dep : clappergtk_deps
  if not dep.found()
    if clappergtk_option.enabled()
      error('clapper-gtk option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

subdir('po')

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name() + '-gtk')
config_h.set_quoted('LOCALEDIR', join_paths (prefix, localedir))
config_h.set_quoted('CLAPPER_GTK_RESOURCE_PREFIX', clappergtk_resource_prefix)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

visibility_conf = configuration_data()

visibility_conf.set(
  'CLAPPER_API',
  'CLAPPER_GTK',
)

clappergtk_visibility_header = configure_file(
  input: '../shared/clapper-api-visibility.h.in',
  output: 'clapper-gtk-visibility.h',
  configuration: visibility_conf,
)

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
  'clapper-gtk-enums.h',
  'clapper-gtk-billboard.h',
  'clapper-gtk-container.h',
  'clapper-gtk-extra-menu-button.h',
  'clapper-gtk-lead-container.h',
  'clapper-gtk-next-item-button.h',
  'clapper-gtk-previous-item-button.h',
  'clapper-gtk-seek-bar.h',
  'clapper-gtk-simple-controls.h',
  'clapper-gtk-title-header.h',
  'clapper-gtk-title-label.h',
  'clapper-gtk-toggle-fullscreen-button.h',
  'clapper-gtk-toggle-play-button.h',
  'clapper-gtk-utils.h',
  'clapper-gtk-video.h',
  clappergtk_version_header,
  clappergtk_visibility_header,
]
clappergtk_sources = [
  'clapper-gtk-billboard.c',
  'clapper-gtk-buffering-animation.c',
  'clapper-gtk-buffering-paintable.c',
  'clapper-gtk-container.c',
  'clapper-gtk-extra-menu-button.c',
  'clapper-gtk-lead-container.c',
  'clapper-gtk-limited-layout.c',
  'clapper-gtk-next-item-button.c',
  'clapper-gtk-previous-item-button.c',
  'clapper-gtk-seek-bar.c',
  'clapper-gtk-simple-controls.c',
  'clapper-gtk-status.c',
  'clapper-gtk-stream-check-button.c',
  'clapper-gtk-title-header.c',
  'clapper-gtk-title-label.c',
  'clapper-gtk-toggle-fullscreen-button.c',
  'clapper-gtk-toggle-play-button.c',
  'clapper-gtk-utils.c',
  'clapper-gtk-video.c',
  'clapper-gtk-video-placeholder.c',
  clappergtk_resources,
]
clappergtk_c_args = [
  '-DG_LOG_DOMAIN="ClapperGtk"',
  '-DCLAPPER_GTK_COMPILATION',
  '-DGST_USE_UNSTABLE_API',
]

if get_option('default_library') == 'static'
  clappergtk_c_args += ['-DCLAPPER_GTK_STATIC_COMPILATION']
endif

clappergtk_headers_dir = join_paths(includedir, clapper_api_name, 'clapper-gtk')

clappergtk_enums = gnome.mkenums_simple(
  'clapper-gtk-enum-types',
  sources: clappergtk_headers,
  decorator: 'CLAPPER_GTK_API',
  header_prefix: '#include <clapper-gtk/clapper-gtk-visibility.h>',
  identifier_prefix: 'ClapperGtk',
  symbol_prefix: 'clapper_gtk',
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
    clappergtk_visibility_header,
    clappergtk_version_header,
    clappergtk_enums[1],
  ],
)
