clapper_features_headers = []
clapper_features_sources = []

subdir('mpris')

features_availability_conf = configuration_data()

features_availability_conf.set(
  'CLAPPER_HAVE_MPRIS',
  clapper_have_mpris ? 'TRUE' : 'FALSE',
)

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
