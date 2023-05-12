feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

# Known OSes that can support our MPRIS implementation
os_supported = ['linux'].contains(host_machine.system())

if not os_supported
  if feature_option.enabled()
    error('@0@ feature was enabled, but OS is not supported by it'.format(feature_name))
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
]
clapper_features_sources_internal += [
  clapper_mpris_gdbus,
]

install_headers('clapper-mpris.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'mpris'),
)
clapper_available_features += feature_name
