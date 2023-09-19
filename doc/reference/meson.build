doc_version_conf = configuration_data()
doc_version_conf.set('CLAPPER_VERSION', meson.project_version())
doc_version_conf.set('CLAPPER_VERSION_SUFFIX', clapper_version_suffix)

gi_docgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]
if get_option('werror')
  gi_docgen_common_args += ['--fatal-warnings']
endif

if build_clapper
  subdir('clapper')
endif
if build_clappergtk
  subdir('clapper-gtk')
endif
