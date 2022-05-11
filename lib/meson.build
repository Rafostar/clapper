glib_req = '>= 2.68.0'
gst_req = '>= 1.20.0'

api_version = '1.0'
libversion = meson.project_version()

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

cdata = configuration_data()

os_unix = host_machine.system() != 'windows'

if cc.get_id() == 'msvc'
  msvc_args = [
    # Ignore several spurious warnings for things gstreamer does very commonly
    # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
    # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
    # NOTE: Only add warnings here if you are sure they're spurious
    '/wd4018', # implicit signed/unsigned conversion
    '/wd4146', # unary minus on unsigned (beware INT_MIN)
    '/wd4244', # lossy type conversion (e.g. double -> int)
    '/wd4305', # truncating type conversion (e.g. double -> float)
    cc.get_supported_arguments(['/utf-8']), # set the input encoding to utf-8

    # Enable some warnings on MSVC to match GCC/Clang behaviour
    '/w14062', # enumerator 'identifier' in switch of enum 'enumeration' is not handled
    '/w14101', # 'identifier' : unreferenced local variable
    '/w14189', # 'identifier' : local variable is initialized but not referenced
  ]
  add_project_arguments(msvc_args, language: ['c', 'cpp'])
  noseh_link_args = ['/SAFESEH:NO']
else
  if cxx.has_argument('-Wno-non-virtual-dtor')
    add_project_arguments('-Wno-non-virtual-dtor', language: 'cpp')
  endif
  noseh_link_args = []
endif

if cc.has_link_argument('-Wl,-Bsymbolic-functions')
  add_project_link_arguments('-Wl,-Bsymbolic-functions', language: 'c')
endif

# Symbol visibility
if cc.get_id() == 'msvc'
  export_define = '__declspec(dllexport) extern'
else
  export_define = 'extern'
endif

# Passing this through the command line would be too messy
cdata.set('GST_API_EXPORT', export_define)

# Disable strict aliasing
if cc.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'c')
endif
if cxx.has_argument('-fno-strict-aliasing')
  add_project_arguments('-fno-strict-aliasing', language: 'cpp')
endif

if not get_option('deprecated-glib-api')
  message('Disabling deprecated GLib API')
  add_project_arguments('-DG_DISABLE_DEPRECATED', language: 'c')
endif

if not get_option('devel-checks')
  message('Disabling GLib cast checks')
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: 'c')

  message('Disabling GLib asserts')
  add_project_arguments('-DG_DISABLE_ASSERT', language: 'c')

  message('Disabling GLib checks')
  add_project_arguments('-DG_DISABLE_CHECKS', language: 'c')
endif

check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_FCNTL_H', 'fcntl.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_NETINET_IN_H', 'netinet/in.h'],
  ['HAVE_NETINET_IP_H', 'netinet/ip.h'],
  ['HAVE_NETINET_TCP_H', 'netinet/tcp.h'],
  ['HAVE_PTHREAD_H', 'pthread.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_PARAM_H', 'sys/param.h'],
  ['HAVE_SYS_SOCKET_H', 'sys/socket.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TIME_H', 'sys/time.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_SYS_UTSNAME_H', 'sys/utsname.h'],
  ['HAVE_UNISTD_H', 'unistd.h'],
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    cdata.set(h.get(0), 1)
  endif
endforeach

check_functions = [
  ['HAVE_DCGETTEXT', 'dcgettext'],
  ['HAVE_GETPAGESIZE', 'getpagesize'],
  ['HAVE_GMTIME_R', 'gmtime_r'],
  ['HAVE_MEMFD_CREATE', 'memfd_create'],
  ['HAVE_MMAP', 'mmap'],
  ['HAVE_PIPE2', 'pipe2'],
  ['HAVE_GETRUSAGE', 'getrusage', '#include<sys/resource.h>'],
]

foreach f : check_functions
  prefix = ''
  if f.length() == 3
    prefix = f.get(2)
  endif
  if cc.has_function(f.get(1), prefix: prefix)
    cdata.set(f.get(0), 1)
  endif
endforeach

cdata.set('SIZEOF_CHAR', cc.sizeof('char'))
cdata.set('SIZEOF_INT', cc.sizeof('int'))
cdata.set('SIZEOF_LONG', cc.sizeof('long'))
cdata.set('SIZEOF_SHORT', cc.sizeof('short'))
cdata.set('SIZEOF_VOIDP', cc.sizeof('void*'))

cdata.set_quoted('VERSION', libversion)
cdata.set_quoted('PACKAGE', 'clapper')
cdata.set_quoted('PACKAGE_VERSION', libversion)
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/Rafostar/clapper/issues/new')
cdata.set_quoted('PACKAGE_NAME', 'GStreamer Clapper Libs')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('LIBDIR', pkglibdir)
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))

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
]
warning_c_flags = [
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
]
warning_cxx_flags = [
  '-Wformat-nonliteral',
]

foreach extra_arg : warning_c_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

foreach extra_arg : warning_cxx_flags
  if cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
  if cxx.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'cpp')
  endif
endforeach

cdata.set_quoted('GST_PACKAGE_NAME', 'gst-plugin-clapper')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/Rafostar/clapper')

# Mandatory GST deps
gst_dep = dependency('gstreamer-1.0', version: gst_req,
  fallback: ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version: gst_req,
  fallback: ['gstreamer', 'gst_base_dep'])
gstpbutils_dep = dependency('gstreamer-pbutils-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'pbutils_dep'])
gstaudio_dep = dependency('gstreamer-audio-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'audio_dep'])
gsttag_dep = dependency('gstreamer-tag-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'tag_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'video_dep'])

# GStreamer OpenGL
gstgl_dep = dependency('gstreamer-gl-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'gstgl_dep'], required: true)
gstglx11_dep = dependency('', required: false)
gstglwayland_dep = dependency('', required: false)
gstglegl_dep = dependency('', required: false)

gst_gl_apis = gstgl_dep.get_pkgconfig_variable('gl_apis')
gst_gl_winsys = gstgl_dep.get_pkgconfig_variable('gl_winsys')
gst_gl_platforms = gstgl_dep.get_pkgconfig_variable('gl_platforms')

message('GStreamer OpenGL window systems: @0@'.format(gst_gl_winsys))
message('GStreamer OpenGL platforms: @0@'.format(gst_gl_platforms))
message('GStreamer OpenGL apis: @0@'.format(gst_gl_apis))

foreach ws : ['x11', 'wayland', 'android', 'cocoa', 'eagl', 'win32', 'dispmanx', 'viv_fb']
  set_variable('gst_gl_have_window_@0@'.format(ws), gst_gl_winsys.contains(ws))
endforeach

foreach p : ['glx', 'egl', 'cgl', 'eagl', 'wgl']
  set_variable('gst_gl_have_platform_@0@'.format(p), gst_gl_platforms.contains(p))
endforeach

foreach api : ['gl', 'gles2']
  set_variable('gst_gl_have_api_@0@'.format(api), gst_gl_apis.contains(api))
endforeach

gstglproto_dep = dependency('gstreamer-gl-prototypes-1.0', version: gst_req,
  fallback: ['gst-plugins-base', 'gstglproto_dep'], required: true)
if gst_gl_have_window_x11
  gstglx11_dep = dependency('gstreamer-gl-x11-1.0', version: gst_req,
    fallback: ['gst-plugins-base', 'gstglx11_dep'], required: true)
endif
if gst_gl_have_window_wayland
  gstglwayland_dep = dependency('gstreamer-gl-wayland-1.0', version: gst_req,
    fallback: ['gst-plugins-base', 'gstglwayland_dep'], required: true)
endif
if gst_gl_have_platform_egl
  gstglegl_dep = dependency('gstreamer-gl-egl-1.0', version: gst_req,
    fallback: ['gst-plugins-base', 'gstglegl_dep'], required: true)
endif

libm = cc.find_library('m', required: false)
glib_dep = dependency('glib-2.0', version: glib_req, fallback: ['glib', 'libglib_dep'])
gmodule_dep = dependency('gmodule-2.0', fallback: ['glib', 'libgmodule_dep'])
gio_dep = dependency('gio-2.0', fallback: ['glib', 'libgio_dep'])

if os_unix
  giounix_dep = dependency('gio-unix-2.0', version: glib_req, fallback: ['glib', 'libgio_dep'])
else
  giowin_dep = dependency('gio-windows-2.0', version: glib_req, fallback : ['glib', 'libgio_dep'])
endif

cdata.set('DISABLE_ORC', 1)
cdata.set('GST_ENABLE_EXTRA_CHECKS', get_option('devel-checks'))

configinc = include_directories('.')
libsinc = include_directories('gst')

gir = find_program('g-ir-scanner', required: false)
gir_init_section = ['--add-init-section=extern void gst_init(gint*,gchar**);' + \
    'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
    'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
    'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
    'gst_init(NULL,NULL);', '--quiet'
]

gst_clapper_plugin_libdir = join_paths(get_option('prefix'), libdir, 'clapper-@0@'.format(api_version), 'gst', 'plugin')
gst_clapper_importers_libdir = join_paths(gst_clapper_plugin_libdir, 'importers')
cdata.set_quoted('CLAPPER_SINK_IMPORTER_PATH', gst_clapper_importers_libdir)

subdir('gst')
configure_file(output: 'config.h', configuration: cdata)
