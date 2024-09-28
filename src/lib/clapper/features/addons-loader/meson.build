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

clapper_features_headers += [
  'features/addons-loader/clapper-addons-loader.h',
]
clapper_features_sources += [
  'features/addons-loader/clapper-addons-loader.c',
]
clapper_features_deps += feature_deps

install_headers('clapper-addons-loader.h',
  install_dir: join_paths(clapper_headers_dir, 'features', 'addons-loader'),
)
clapper_available_features += feature_name
