doc_version_conf = configuration_data()
doc_version_conf.set('CLAPPER_VERSION', meson.project_version())

gi_docgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]
if get_option('werror')
  gi_docgen_common_args += ['--fatal-warnings']
endif

subdir('clapper')
