feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

feature_deps = [
  dependency('libsoup-3.0', required: false),
  dependency('microdns', version: '>= 0.2.0', required: false),
]
foreach dep : feature_deps
  if not dep.found()
    if feature_option.enabled()
      error('@0@ feature was enabled, but required dependencies were not found'.format(feature_name))
    endif
    subdir_done()
  endif
endforeach

clapper_features_headers += [
  'features/server/clapper-server.h',
]
clapper_features_sources += [
  'features/server/clapper-server.c',
  'features/server/clapper-server-json.c',
  'features/server/clapper-server-actions.c',
  'features/server/clapper-server-mdns.c',
]
clapper_features_deps += feature_deps

install_headers('clapper-server.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'server'),
)
clapper_available_features += feature_name
