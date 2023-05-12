mpris_option = get_option('mpris')
clapper_have_mpris = false

if mpris_option.disabled()
  subdir_done()
endif

# Known OSes that can support our MPRIS implementation
os_supported = ['linux'].contains(host_machine.system())

if not os_supported
  if mpris_option.enabled()
    error('"mpris" option was enabled, but OS is not supported by it')
  endif
  subdir_done()
endif

clapper_mpris_gdbus = gnome.gdbus_codegen('clapper-mpris-gdbus',
  sources: 'clapper-mpris-gdbus.xml',
  interface_prefix: 'org.mpris.',
  namespace: 'ClapperMpris',
)
clapper_features_headers += [
  'features/mpris/clapper-mpris.h',
]
clapper_features_sources += [
  'features/mpris/clapper-mpris.c',
  clapper_mpris_gdbus,
]

install_headers('clapper-mpris.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'mpris'),
)
clapper_have_mpris = true
