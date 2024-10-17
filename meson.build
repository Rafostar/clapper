project('clapper', 'c',
  version: '0.7.0',
  meson_version: '>= 0.64.0',
  license: 'LGPL-2.1-or-later AND GPL-3.0-or-later', # LGPL-2.1+ for libs and gst-plugin, GPL-3.0+ for app
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized',
  ],
)

glib_req = '>= 2.76.0'
gst_req = '>= 1.20.0'
gtk4_req = '>= 4.10.0'
adw_req = '>= 1.5.0'

clapper_version = meson.project_version().split('-')[0]
version_array = clapper_version.split('.')
clapper_version_suffix = '-' + version_array[0] + '.0'

clapper_api_name = meson.project_name() + clapper_version_suffix

devenv = environment()

gnome = import('gnome')
pkgconfig = import('pkgconfig')
i18n = import('i18n')

prefix = get_option('prefix')
bindir = get_option('bindir')
datadir = get_option('datadir')
libdir = get_option('libdir')
localedir = get_option('localedir')
includedir = get_option('includedir')
optimization = get_option('optimization')

clapper_libdir = join_paths(prefix, libdir, clapper_api_name)
build_optimized = optimization in ['2', '3', 's']

gst_dep = dependency('gstreamer-1.0',
  version: gst_req,
  required: false,
)
gst_base_dep = dependency('gstreamer-base-1.0',
  version: gst_req,
  required: false,
)
gst_video_dep = dependency('gstreamer-video-1.0',
  version: gst_req,
  required: false,
)
gst_audio_dep = dependency('gstreamer-audio-1.0',
  version: gst_req,
  required: false,
)
gst_pbutils_dep = dependency('gstreamer-pbutils-1.0',
  version: gst_req,
  required: false,
)
gst_tag_dep = dependency('gstreamer-tag-1.0',
  version: gst_req,
  required: false,
)
glib_dep = dependency('glib-2.0',
  version: glib_req,
  required: false,
)
gobject_dep = dependency('gobject-2.0',
  version: glib_req,
  required: false,
)
gio_dep = dependency('gio-2.0',
  version: glib_req,
  required: false,
)
gmodule_dep = dependency('gmodule-2.0',
  version: glib_req,
  required: false,
)
gtk4_dep = dependency('gtk4',
  version: gtk4_req,
  required: false,
)
libadwaita_dep = dependency('libadwaita-1',
  version: adw_req,
  required: false,
)

cc = meson.get_compiler('c')
libm = cc.find_library('m', required: false)

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Wformat',
  '-Wformat-security',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Waddress',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
  '-Wsign-compare',
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

if build_optimized
  message('Disabling GLib cast checks')
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')

  message('Disabling GLib asserts')
  add_project_arguments('-DG_DISABLE_ASSERT', language: 'c')

  message('Disabling GLib checks')
  add_project_arguments('-DG_DISABLE_CHECKS', language: 'c')
endif

subdir('src')
subdir('doc')

meson.add_devenv(devenv)

summary({
  'prefix': prefix,
  'bindir': bindir,
  'datadir': datadir,
  'libdir': libdir,
  'localedir': localedir,
  'includedir': includedir,
  'optimization': optimization,
}, section: 'Directories')

summary('clapper', build_clapper ? 'Yes' : 'No', section: 'Build')
summary('clapper-gtk', build_clappergtk ? 'Yes' : 'No', section: 'Build')
summary('clapper-app', build_clapperapp ? 'Yes' : 'No', section: 'Build')
summary('gst-plugin', build_gst_plugin ? 'Yes' : 'No', section: 'Build')
summary('introspection', build_gir ? 'Yes' : 'No', section: 'Build')
summary('vapi', build_vapi ? 'Yes' : 'No', section: 'Build')
summary('doc', build_doc  ? 'Yes' : 'No', section: 'Build')

if build_clapper
  foreach name : clapper_possible_features
    summary(name, clapper_available_features.contains(name) ? 'Yes' : 'No', section: 'Features')
  endforeach
endif
