clapper_features_headers = []
clapper_features_sources = []
clapper_features_sources_internal = []
clapper_features_deps = []
clapper_available_features = []

features_availability_conf = configuration_data()

clapper_possible_features = [
  'discoverer',
  'mpris',
  'server',
]
foreach feature_name : clapper_possible_features
  subdir(feature_name)
  features_availability_conf.set(
    'CLAPPER_HAVE_@0@'.format(feature_name.replace('-', '_').to_upper()),
    clapper_available_features.contains(feature_name) ? 'TRUE' : 'FALSE'
  )
endforeach

clapper_features_availability_header = configure_file(
  input: 'clapper-features-availability.h.in',
  output: 'clapper-features-availability.h',
  configuration: features_availability_conf,
)
install_headers(clapper_features_availability_header,
  install_dir: join_paths(clapper_headers_dir, 'features'),
)
clapper_features_headers += [
  clapper_features_availability_header
]
