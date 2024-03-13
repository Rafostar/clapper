feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

clapper_features_headers += [
  'features/discoverer/clapper-discoverer.h',
]
clapper_features_sources += [
  'features/discoverer/clapper-discoverer.c',
]

install_headers('clapper-discoverer.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'discoverer'),
)
clapper_available_features += feature_name
