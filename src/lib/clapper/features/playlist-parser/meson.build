feature_option = get_option(feature_name)

if feature_option.disabled()
  subdir_done()
endif

feature_deps = [
  dependency('libpeas-2', required: false)
]
foreach dep : feature_deps
  if not dep.found()
    if feature_option.enabled()
      error('@0@ feature was enabled, but required dependencies were not found'.format(feature_name))
    endif
    subdir_done()
  endif
endforeach

# FIXME
subdir_done()

clapper_features_headers += [
  'features/playlist-parser/clapper-playlist-parser.h',
]
clapper_features_sources += [
  'features/playlist-parser/clapper-playlist-parser.c',
  '../shared/clapper-addons-loader.c',
]
clapper_features_deps += feature_deps

install_headers('clapper-playlist-parser.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'playlist-parser'),
)
clapper_available_features += feature_name
